/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/display/ice40lp/snowy_boot.h"
#include "drivers/display/ice40lp/ice40lp_definitions.h"
#include "drivers/display/ice40lp/ice40lp_internal.h"

#include "board/board.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/pmic.h"
#include "drivers/spi.h"
#include "system/logging.h"
#include "system/passert.h"
#include "kernel/util/delay.h"

#include <stdbool.h>
#include <stdint.h>

#define CMD_NULL (0)
#define CMD_SET_PARAMETER (1)
#define CMD_DISPLAY_OFF (2)
#define CMD_DISPLAY_ON (3)
#define CMD_DRAW_SCENE (4)

#define SCENE_BLACK (0)
#define SCENE_SPLASH (1)
#define SCENE_UPDATE (2)
#define SCENE_ERROR (3)

#define UPDATE_PROGRESS_MAX (93)

static void prv_start_command(uint8_t cmd) {
  display_spi_begin_transaction();
  spi_ll_slave_write(ICE40LP->spi_port, cmd);
}

static void prv_send_command_arg(uint8_t arg) {
  spi_ll_slave_write(ICE40LP->spi_port, arg);
}

static void prv_end_command(void) {
  display_spi_end_transaction();
}

static bool prv_wait_busy(void) {
  // The display should come out of busy within 35 milliseconds;
  // it is a waste of time to wait more than twice that.
  int timeout = 50 * 10;
  while (display_busy()) {
    if (timeout-- == 0) {
      PBL_LOG(LOG_LEVEL_ERROR, "Display busy-wait timeout expired!");
      return false;
    }
    delay_us(100);
  }
  return true;
}

static void prv_screen_on(void) {
  prv_start_command(CMD_DISPLAY_ON);
  prv_end_command();
}

static void prv_screen_off(void) {
  prv_start_command(CMD_DISPLAY_OFF);
  prv_end_command();
}

void prv_draw_scene(uint8_t scene) {
  prv_start_command(CMD_DRAW_SCENE);
  prv_send_command_arg(scene);
  prv_end_command();
}
void prv_set_parameter(uint32_t param) {
  prv_start_command(CMD_SET_PARAMETER);
  // Send in little-endian byte order
  prv_send_command_arg(param & 0xff);
  prv_send_command_arg((param >> 8) & 0xff);
  prv_send_command_arg((param >> 16) & 0xff);
  prv_send_command_arg((param >> 24) & 0xff);
  prv_end_command();
}

void boot_display_show_boot_splash(void) {
  prv_wait_busy();
  prv_draw_scene(SCENE_SPLASH);
  // Don't turn the screen on until the boot-splash is fully drawn.
  prv_wait_busy();
  prv_screen_on();
}

void boot_display_show_firmware_update_progress(
    uint32_t numerator, uint32_t denominator) {
  static uint8_t last_bar_fill = UINT8_MAX;
  // Scale progress to the number of pixels in the progress bar,
  // rounding half upwards.
  uint8_t bar_fill =
      ((numerator * UPDATE_PROGRESS_MAX) + ((denominator+1)/2)) / denominator;
  // Don't waste time and power redrawing the same screen repeatedly.
  if (bar_fill != last_bar_fill) {
    last_bar_fill = bar_fill;
    prv_set_parameter(bar_fill);
    prv_draw_scene(SCENE_UPDATE);
  }
}

bool boot_display_show_error_code(uint32_t error_code) {
  prv_set_parameter(error_code);
  prv_draw_scene(SCENE_ERROR);
  if (prv_wait_busy()) {
    prv_screen_on();
    return true;
  } else {
    return false;
  }
}

void boot_display_screen_off(void) {
  prv_screen_off();
  prv_draw_scene(SCENE_BLACK);
  prv_wait_busy();
}
