# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

This is a CMake + Pico SDK project targeting RP2040/RP2350. Builds via VSCode (Raspberry Pi Pico extension) or manually:

```bash
# One-time setup (Linux)
sudo apt install gcc-arm-none-eabi libnewlib-dev libnewlib-arm-none-eabi ninja-build cmake

# Build
PICO_SDK_FETCH_FROM_GIT=1 PICO_SDK_FETCH_FROM_GIT_TAG=2.2.0 cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build

# Build for Pico2
cmake --fresh -B build -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel -DPICO_BOARD=pico2
cmake --build build
```

VSCode users install the `Raspberry Pi Pico` extension; it manages the SDK, toolchain, CMake, and Ninja. Build via the extension's command palette or the CMake status bar.

## Architecture — Three Git Submodules

| Layer | Submodule | Role | Depends on |
|-------|-----------|------|------------|
| **Core** | `u2hts_core/` | Controller detection, touch→HID mapping, coordinate transforms, USB reporting logic, HID report descriptor definitions | Nothing (pure C) |
| **Touch controllers** | `u2hts_touch_controllers/` | Per-chip I2C/SPI drivers (GT9xx, FT54x6, RMI4, CST8xx, etc.) | `u2hts_core` |
| **Platform (RP2040)** | _this repo's `src/` + `include/`_ | `u2hts_rp2.c` implements `u2hts_api.h` (I2C, SPI, GPIO, USB via TinyUSB, flash config). `u2hts_main.c` is the entry point. | Both submodules + Pico SDK |

The core and touch-controller layers contain **zero RP2040 SDK includes**. All hardware interaction goes through the function pointers declared in `u2hts_core/u2hts_api.h` and implemented by the platform layer.

## Platform Abstraction — `u2hts_api.h`

To port to a new MCU, implement every function in `u2hts_core/u2hts_api.h`:

- **Required**: `u2hts_delay_ms/us`, `u2hts_get_timestamp`, all `u2hts_i2c_*` (or `u2hts_spi_*`), `u2hts_usb_init/report`, `u2hts_tpint_*`, `u2hts_tprst_*`, `u2hts_ts_irq_*`
- **Optional**: `u2hts_led_*` (behind `U2HTS_ENABLE_LED`), `u2hts_write/read_config` (behind `U2HTS_ENABLE_PERSISTENT_CONFIG`), `u2hts_usrkey_get` + `u2hts_key_irq_*` (behind `U2HTS_ENABLE_KEY`), `u2hts_backlight_set`

## Controller Registration

Two mechanisms, both searched transparently by the core:

1. **Linker section** (default for RP2040): `U2HTS_TOUCH_CONTROLLER(name)` macro places a pointer in `.u2hts_touch_controllers`. Requires a custom linker script section exporting `__u2hts_touch_controllers_begin/end`.
2. **Manual registration** (fallback for platforms without the custom section): Call `u2hts_register_controller(&controller)` before `u2hts_init()`. Boundary symbols are `__attribute__((weak))` — they default to NULL when the linker doesn't define them.

All three lookup functions (`by_name`, `by_i2c_addr`, `list`) search both sources.

## Adding a Touch Controller Driver

1. Create a `.c` file in `u2hts_touch_controllers/`
2. Include `"u2hts_core.h"` only — no platform headers
3. Implement `u2hts_touch_controller_operations` (`.setup`, `.fetch`, `.get_config` if supported)
4. Define a `u2hts_touch_controller` struct with name, IRQ type, report mode, I2C/SPI config
5. Call `U2HTS_TOUCH_CONTROLLER(your_controller)` at file scope
6. Add the `.c` file to `u2hts_touch_controllers/CMakeLists.txt`

Controller drivers use `u2hts_i2c_mem_read/write(core, addr, reg, reg_size, data, len)` for I2C register access — never call platform I2C functions directly.

## Key Compile-Time Flags

Set on `u2hts_core_settings` (INTERFACE target, propagates to all consumers):

| Flag | Effect |
|------|--------|
| `U2HTS_MAX_TPS=N` | Touch points in HID report descriptor (default 10) |
| `U2HTS_ENABLE_LED` | LED status/error indication |
| `U2HTS_ENABLE_KEY` | Hardware key for runtime config switching |
| `U2HTS_ENABLE_PERSISTENT_CONFIG` | Save config to flash |
| `U2HTS_ENABLE_COMPACT_REPORT` | 12-bit X/Y (no width/height/pressure) to reduce report size |
| `U2HTS_LOG_LEVEL` | `U2HTS_LOG_LEVEL_{ERROR,WARN,INFO,DEBUG}` (default INFO) |

## HID Report Descriptor

Defined as raw-byte macros in `u2hts_core/u2hts_hid_report_descriptor.h`. The CMake function `u2hts_generate_hid_report_descriptor()` auto-generates the `U2HTS_HID_REPORT_DESCRIPTOR` compile definition with the correct number of touch points. The platform layer references this single macro in `u2hts_rp2.c` — no TinyUSB-specific HID macros exist outside the platform layer.

## RP2040-Specific Config

`u2hts_main.c` uses `picotool` binary-info declarations (`bi_decl`) to expose config as UF2 metadata. Users can change touchscreen settings without recompiling:

```bash
picotool config -s x_invert 1 build/U2HTS.uf2
picotool load -f build/U2HTS.uf2
```

Custom linker scripts (`memmap_rp2040.ld`, `memmap_rp2350.ld`) add the `.u2hts_touch_controllers` section for auto-discovery of controller drivers.
