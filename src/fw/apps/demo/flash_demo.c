/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "flash_demo.h"

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"

#include <pbl/drivers/flash.h>
#include "flash_region/flash_region.h"
#include <pbl/logging/logging.h>

static Window *window;

#define BASE_ADDRESS 0x380000

static void test_write_short(void) {
  uint16_t buffer;
  pbl_flash_read(FLASH, BASE_ADDRESS, (uint8_t*) &buffer, sizeof(buffer));
  PBL_LOG_DBG(">> Addr 0x%x is 0x%"PRIx16, BASE_ADDRESS, buffer);

  buffer = 0x0505;
  pbl_flash_write(FLASH, BASE_ADDRESS, (uint8_t*) &buffer, sizeof(buffer));
  PBL_LOG_DBG(">> Addr 0x%x Written to 0x%x", BASE_ADDRESS, buffer);

  uint8_t read_buffer = 0x0;
  pbl_flash_read(FLASH, BASE_ADDRESS, (uint8_t*) &read_buffer, sizeof(read_buffer));
  PBL_LOG_DBG(">> Addr 0x%x is (8) 0x%"PRIx8, BASE_ADDRESS, read_buffer);

  buffer = 0x0;
  pbl_flash_read(FLASH, BASE_ADDRESS, (uint8_t*) &buffer, sizeof(buffer));
  PBL_LOG_DBG(">> Addr 0x%x is (16) 0x%"PRIx16, BASE_ADDRESS, buffer);
}

static void test_write_bytes(void) {
  for (int i = 1; i < 127; ++i) {
    uint8_t data = i;
    pbl_flash_write(FLASH, BASE_ADDRESS + i, (uint8_t*) &data, sizeof(data));
    PBL_LOG_DBG(">> Wrote Addr 0x%x is 0x%"PRIx8, i, data);
  }

  for (int i = 0; i < 128; ++i) {
    uint8_t data = 0;
    pbl_flash_read(FLASH, BASE_ADDRESS + i, (uint8_t*) &data, sizeof(data));
    PBL_LOG_DBG(">> Read Addr 0x%x is (8) 0x%"PRIx8, i, data);
  }
}

static void test_write_block(void) {
  uint8_t data[64];

  for (unsigned int i = 0; i < sizeof(data); ++i) {
    data[i] = i;
  }

  pbl_flash_write(FLASH, BASE_ADDRESS + 31, data, sizeof(data));

  for (int i = 0; i < 128; ++i) {
    uint8_t data = 0;
    pbl_flash_read(FLASH, BASE_ADDRESS + i, (uint8_t*) &data, sizeof(data));
    PBL_LOG_DBG(">> Read Addr 0x%x is (8) 0x%"PRIx8, i, data);
  }
}

static void do_flash_operation(void) {
  PBL_LOG_DBG(">> Flash operation time!");
  PBL_LOG_DBG(">> Flash operation time!");
  PBL_LOG_DBG(">> Flash operation time!");
  PBL_LOG_DBG(">> Flash operation time!");
  PBL_LOG_DBG(">> Flash operation time!");
  PBL_LOG_DBG(">> Flash operation time!");

  PBL_LOG_DBG(">> Erasing 0x%x", BASE_ADDRESS);
  pbl_flash_erase(FLASH, BASE_ADDRESS, SECTOR_SIZE_BYTES);
  PBL_LOG_DBG(">> Erasing 0x%x Done", BASE_ADDRESS);

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

