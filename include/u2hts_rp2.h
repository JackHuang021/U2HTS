/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
*/

#ifndef _U2HTS_RP2_H_
#define _U2HTS_RP2_H_

#include <bsp/board_api.h>
#include <hardware/flash.h>
#include <hardware/i2c.h>
#include <pico/flash.h>
#include <pico/stdlib.h>
#include <tusb.h>

#include "tusb_config.h"

#define U2HTS_SWAP16(x) __builtin_bswap16(x)
#define U2HTS_SWAP32(x) __builtin_bswap32(x)

#ifndef PICO_DEFAULT_I2C
#define U2HTS_I2C i2c1
#else
#define U2HTS_I2C i2c_default
#endif

#define U2HTS_I2C_TIMEOUT 10 * 1000  // 10ms

#ifndef PICO_DEFAULT_I2C_SDA_PIN
#define U2HTS_I2C_SDA 4
#else
#define U2HTS_I2C_SDA PICO_DEFAULT_I2C_SDA_PIN
#endif

#ifndef PICO_DEFAULT_I2C_SCL_PIN
#define U2HTS_I2C_SCL 5
#else
#define U2HTS_I2C_SCL PICO_DEFAULT_I2C_SCL_PIN
#endif

#ifndef U2HTS_TP_INT
#define U2HTS_TP_INT 2
#endif

#ifndef U2HTS_TP_RST
#define U2HTS_TP_RST 3
#endif

#ifndef U2HTS_USR_KEY
#define U2HTS_USR_KEY 6
#endif

// last page
#define U2HTS_CONFIG_STORAGE_OFFSET PICO_FLASH_SIZE_BYTES - 8192

inline static void u2hts_pins_init() {
  // some touch contoller requires ATTN signal in specified state while
  // resetting.
  gpio_set_function(U2HTS_TP_INT, GPIO_FUNC_SIO);
  gpio_set_dir(U2HTS_TP_INT, GPIO_OUT);
  gpio_put(U2HTS_TP_INT, true);

  gpio_set_function(U2HTS_TP_RST, GPIO_FUNC_SIO);
  gpio_set_dir(U2HTS_TP_RST, GPIO_OUT);
  gpio_put(U2HTS_TP_RST, true);

  gpio_set_function(PICO_DEFAULT_LED_PIN, GPIO_FUNC_SIO);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  gpio_init(U2HTS_USR_KEY);
  gpio_set_dir(U2HTS_USR_KEY, GPIO_IN);
}
#endif