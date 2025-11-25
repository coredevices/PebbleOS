/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "flash_demo.h"

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"

#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "system/logging.h"

static Window *window;

#define BASE_ADDRESS 0x380000

static void test_write_short(void) {
  uint16_t buffer;
  flash_read_bytes((uint8_t*) &buffer, BASE_ADDRESS, sizeof(buffer));
  PBL_LOG(LOG_LEVEL_DEBUG, ">> Addr 0x%x is 0x%"PRIx16, BASE_ADDRESS, buffer);

  buffer = 0x0505;
  flash_write_bytes((uint8_t*) &buffer, BASE_ADDRESS, sizeof(buffer));
  PBL_LOG(LOG_LEVEL_DEBUG, ">> Addr 0x%x Written to 0x%x", BASE_ADDRESS, buffer);

  uint8_t read_buffer = 0x0;
  flash_read_bytes((uint8_t*) &read_buffer, BASE_ADDRESS, sizeof(read_buffer));
  PBL_LOG(LOG_LEVEL_DEBUG, ">> Addr 0x%x is (8) 0x%"PRIx8, BASE_ADDRESS, read_buffer);

  buffer = 0x0;
  flash_read_bytes((uint8_t*) &buffer, BASE_ADDRESS, sizeof(buffer));
  PBL_LOG(LOG_LEVEL_DEBUG, ">> Addr 0x%x is (16) 0x%"PRIx16, BASE_ADDRESS, buffer);
}

static void test_write_bytes(void) {
  for (int i = 1; i < 127; ++i) {
    uint8_t data = i;
    flash_write_bytes((uint8_t*) &data, BASE_ADDRESS + i, sizeof(data));
    PBL_LOG(LOG_LEVEL_DEBUG, ">> Wrote Addr 0x%x is 0x%"PRIx8, i, data);
  }

  for (int i = 0; i < 128; ++i) {
    uint8_t data = 0;
    flash_read_bytes((uint8_t*) &data, BASE_ADDRESS + i, sizeof(data));
    PBL_LOG(LOG_LEVEL_DEBUG, ">> Read Addr 0x%x is (8) 0x%"PRIx8, i, data);
  }
}

static void test_write_block(void) {
  uint8_t data[64];

  for (unsigned int i = 0; i < sizeof(data); ++i) {
    data[i] = i;
  }

  flash_write_bytes(data, BASE_ADDRESS + 31, sizeof(data));

  for (int i = 0; i < 128; ++i) {
    uint8_t data = 0;
    flash_read_bytes((uint8_t*) &data, BASE_ADDRESS + i, sizeof(data));
    PBL_LOG(LOG_LEVEL_DEBUG, ">> Read Addr 0x%x is (8) 0x%"PRIx8, i, data);
  }
}

static void do_flash_operation(void) {
  PBL_LOG(LOG_LEVEL_DEBUG, ">> Flash operation time!");
  PBL_LOG(LOG_LEVEL_DEBUG, ">> Flash operation time!");
  PBL_LOG(LOG_LEVEL_DEBUG, ">> Flash operation time!");
  PBL_LOG(LOG_LEVEL_DEBUG, ">> Flash operation time!");
  PBL_LOG(LOG_LEVEL_DEBUG, ">> Flash operation time!");
  PBL_LOG(LOG_LEVEL_DEBUG, ">> Flash operation time!");

  PBL_LOG(LOG_LEVEL_DEBUG, ">> Erasing 0x%x", BASE_ADDRESS);
  flash_erase_sector_blocking(BASE_ADDRESS);
  PBL_LOG(LOG_LEVEL_DEBUG, ">> Erasing 0x%x Done", BASE_ADDRESS);

  test_write_short();
}

static void s_main(void) {
  window = window_create();
  app_window_stack_push(window, true /* Animated */);

  do_flash_operation();

  app_event_loop();
}

const PebbleProcessMd* flash_demo_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    .name = "Flash Demo"
  };

  return (const PebbleProcessMd*) &s_app_info;
}

