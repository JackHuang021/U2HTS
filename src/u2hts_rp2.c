/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_rp2.h"

#include "u2hts_core.h"
#include "u2hts_hid_report_descriptor.h"

inline bool u2hts_i2c_write(uint8_t slave_addr, void* buf, size_t len,
                            bool stop) {
  return (i2c_write_timeout_us(U2HTS_I2C, slave_addr, (uint8_t*)buf, len, !stop,
                               U2HTS_I2C_TIMEOUT) == len);
}

inline bool u2hts_i2c_read(uint8_t slave_addr, void* buf, size_t len) {
  return (i2c_read_timeout_us(U2HTS_I2C, slave_addr, (uint8_t*)buf, len, false,
                              U2HTS_I2C_TIMEOUT) == len);
}

inline void u2hts_i2c_init(uint32_t speed_hz) {
  gpio_set_function(U2HTS_I2C_SCL, GPIO_FUNC_I2C);
  gpio_set_function(U2HTS_I2C_SDA, GPIO_FUNC_I2C);
  gpio_pull_up(U2HTS_I2C_SDA);
  gpio_pull_up(U2HTS_I2C_SCL);

  i2c_init(U2HTS_I2C, speed_hz);
}

// not implemented
inline void u2hts_spi_init(u2hts_spi_config* config) {}

inline bool u2hts_spi_transfer(void* buf, size_t len) { return false; }

inline void u2hts_tpint_set(bool value) { gpio_put(U2HTS_TP_INT, value); }

inline bool u2hts_i2c_detect_slave(uint8_t addr) {
  uint8_t rx = 0;
  return i2c_read_timeout_us(U2HTS_I2C, addr, &rx, sizeof(rx), false,
                             U2HTS_I2C_TIMEOUT) >= 0;
}

inline void u2hts_tprst_set(bool value) { gpio_put(U2HTS_TP_RST, value); }

inline void u2hts_i2c_set_speed(uint32_t speed_hz) {
  i2c_set_baudrate(U2HTS_I2C, speed_hz);
}

inline void u2hts_delay_ms(uint32_t ms) { sleep_ms(ms); }
inline void u2hts_delay_us(uint32_t us) { sleep_us(us); }

inline void u2hts_usb_init() { tud_init(BOARD_TUD_RHPORT); }

inline uint16_t u2hts_get_timestamp() {
  return (uint16_t)(to_us_since_boot(time_us_64()) / 100);
}

inline void u2hts_led_set(bool on) { gpio_put(PICO_DEFAULT_LED_PIN, on); }

static void u2hts_rp2_flash_erase(void* param) {
  (void)param;
  flash_range_erase(U2HTS_CONFIG_STORAGE_OFFSET, FLASH_SECTOR_SIZE);
}

static void u2hts_rp2_flash_write(void* param) {
  uint8_t flash_program_buf[FLASH_PAGE_SIZE];
  memset(flash_program_buf, 0xFF, FLASH_PAGE_SIZE);
  *(uint16_t*)flash_program_buf = *(uint16_t*)param;
  flash_range_program(U2HTS_CONFIG_STORAGE_OFFSET, flash_program_buf,
                      FLASH_PAGE_SIZE);
}

inline void u2hts_write_config(uint16_t cfg) {
  flash_safe_execute(u2hts_rp2_flash_erase, NULL, 0xFFFF);
  flash_safe_execute(u2hts_rp2_flash_write, &cfg, 0xFFFF);
}

inline uint16_t u2hts_read_config() {
  return *(uint16_t*)(XIP_BASE + U2HTS_CONFIG_STORAGE_OFFSET);
}

inline bool u2hts_usrkey_get() { return gpio_get(U2HTS_USR_KEY); }

inline void u2hts_tpint_set_mode(bool mode, bool pull) {
  gpio_deinit(U2HTS_TP_INT);
  gpio_set_function(U2HTS_TP_INT, GPIO_FUNC_SIO);
  gpio_set_dir(U2HTS_TP_INT, mode);
  pull ? gpio_pull_up(U2HTS_TP_INT) : gpio_pull_down(U2HTS_TP_INT);
}

inline bool u2hts_tpint_get() { return gpio_get(U2HTS_TP_INT); }

static uint32_t real_irq_type = 0x00;
static bool u2hts_usb_status = false;

static const tusb_desc_device_t u2hts_device_desc = {
    .bLength = sizeof(u2hts_device_desc),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = 0x2e8a,  // Raspberry Pi
    .idProduct = U2HTS_USB_PID,
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01};
// clang-format off
static const uint8_t u2hts_hid_report_desc[] = {
    U2HTS_HID_REPORT_DESCRIPTOR
};
// clang-format on

static uint16_t _desc_str[32 + 1] = {0};

static const uint8_t u2hts_config_desc[] = {
    // Config number, interface count, string index, total length, attribute,
    // power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    // Interface number, string index, protocol, report descriptor len, EP In
    // address, endpoint size & polling interval
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(u2hts_hid_report_desc), 0x81, 64, 1)};

static uint8_t const* string_desc_arr[] = {
    (const uint8_t[]){0x09,
                      0x04},  // 0: is supported language is English (0x0409)
    (const uint8_t*)U2HTS_MANU_STR,  // 1: Manufacturer
    (const uint8_t*)U2HTS_PROD_STR,  // 2: Product
    NULL,  // 3: Serials will use unique ID if possible
};

inline uint8_t const* tud_descriptor_device_cb(void) {
  return (uint8_t const*)&u2hts_device_desc;
}

inline uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
  return (uint8_t const*)u2hts_hid_report_desc;
}

inline uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
  return (uint8_t const*)u2hts_config_desc;
}

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long
// enough for transfer to complete
inline uint16_t const* tud_descriptor_string_cb(uint8_t index,
                                                uint16_t langid) {
  (void)langid;
  size_t chr_count;

  switch (index) {
    case 0:
      memcpy(&_desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;

    case 3:
      chr_count = board_usb_get_serial(_desc_str + 1, 32);
      break;

    default:
      // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
      // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

      if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
        return NULL;

      const uint8_t* str = string_desc_arr[index];

      // Cap at max char
      chr_count = strlen((const char*)str);
      size_t const max_count =
          sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;  // -1 for string type
      if (chr_count > max_count) chr_count = max_count;

      // Convert ASCII string into UTF-16
      for (size_t i = 0; i < chr_count; i++) {
        _desc_str[1 + i] = str[i];
      }
      break;
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}

inline void tud_mount_cb(void) { U2HTS_LOG_DEBUG("device mounted"); }

inline void tud_umount_cb(void) { U2HTS_LOG_DEBUG("device unmounted"); }

inline void tud_suspend_cb(bool remote_wakeup_en) {
  U2HTS_LOG_DEBUG("device suspended, rmt_wakeup_en = %d", remote_wakeup_en);
}

inline void tud_resume_cb(void) { U2HTS_LOG_DEBUG("device resumed"); }

inline void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                                  hid_report_type_t report_type,
                                  uint8_t const* buffer, uint16_t bufsize) {
  U2HTS_LOG_DEBUG(
      "Got hid set report request: instance = %d, report_id = %d, report_type "
      "= %d, bufsize = %d",
      instance, report_id, report_type, bufsize);
}

inline uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                      hid_report_type_t report_type,
                                      uint8_t* buffer, uint16_t reqlen) {
  U2HTS_LOG_DEBUG(
      "Got hid get report request: instance = %d, report_id = %d, report_type "
      "= %d, reqlen = %d",
      instance, report_id, report_type, reqlen);
  if (report_type == HID_REPORT_TYPE_FEATURE) {
    switch (report_id) {
      case U2HTS_HID_REPORT_TP_MAX_COUNT_ID:
        buffer[0] = u2hts_get_max_tps();
        break;
      case U2HTS_HID_REPORT_TP_MS_THQA_CERT_ID:
        static const uint8_t thqa_cert[] = {U2HTS_MS_THQA_CERT_PAYLOAD};
        memcpy(buffer, thqa_cert, reqlen);
        u2hts_usb_status = true;
        break;
      default:
        return 0;
    }
  }
  return reqlen;
}

inline void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report,
                                       uint16_t len) {
  U2HTS_LOG_DEBUG("Enter %s", __func__);
  u2hts_usb_status = true;
}

inline void u2hts_ts_irq_set(bool enable) {
  gpio_set_irq_enabled(U2HTS_TP_INT, real_irq_type, enable);
}

inline static void u2hts_rp2_irq_cb(uint gpio, uint32_t event_mask) {
  u2hts_ts_irq_status_set(gpio == U2HTS_TP_INT && (event_mask & real_irq_type));
}

inline void u2hts_ts_irq_init(U2HTS_IRQ_TYPES irq_type) {
  gpio_deinit(U2HTS_TP_INT);
  switch (irq_type) {
    case IRQ_TYPE_LEVEL_LOW:
      real_irq_type = GPIO_IRQ_LEVEL_LOW;
      gpio_pull_up(U2HTS_TP_INT);
      break;
    case IRQ_TYPE_LEVEL_HIGH:
      real_irq_type = GPIO_IRQ_LEVEL_HIGH;
      gpio_pull_down(U2HTS_TP_INT);
      break;
    case IRQ_TYPE_EDGE_RISING:
      real_irq_type = GPIO_IRQ_EDGE_RISE;
      gpio_pull_down(U2HTS_TP_INT);
      break;
    case IRQ_TYPE_EDGE_FALLING:
    default:
      real_irq_type = GPIO_IRQ_EDGE_FALL;
      gpio_pull_up(U2HTS_TP_INT);
      break;
  }
  gpio_set_irq_enabled_with_callback(U2HTS_TP_INT, real_irq_type, true,
                                     u2hts_rp2_irq_cb);
}

inline void u2hts_usb_report(const u2hts_hid_report* report) {
  static_assert(sizeof(u2hts_hid_report) < CFG_TUD_HID_EP_BUFSIZE,
                "u2hts_hid_report is oversized!");
  tud_hid_report(0, report, sizeof(u2hts_hid_report));
  u2hts_usb_status = false;
}

inline bool u2hts_get_usb_status() { return u2hts_usb_status; }