/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <unistd.h>

#include "board/board.h"

#include <pbl/drivers/mic/qemu/mic_definitions.h>
#include <pbl/drivers/uart/posix.h>

static UARTDeviceState s_dbg_uart_state;

static const struct UARTDevice s_dbg_uart = {
  .state = &s_dbg_uart_state,
  .fd = STDOUT_FILENO,
  .console = true,
};

UARTDevice *const DBG_UART = &s_dbg_uart;

static const PosixDisplayDevice s_display = {
  .width = PBL_DISPLAY_WIDTH,
  .height = PBL_DISPLAY_HEIGHT,
};

DisplayDevice *const DISPLAY = &s_display;

static MicDeviceState s_mic_state;
static MicDevice s_mic = {
  .state = &s_mic_state,
  .channels = 1,
};
MicDevice *const MIC = &s_mic;

const BoardConfigPower BOARD_CONFIG_POWER = {
  .low_power_threshold = 2U,
  .battery_capacity_hours = 168U,
};

const BoardConfig BOARD_CONFIG = {
  .backlight_on_percent = 100,
  .ambient_light_dark_threshold = 150,
  .ambient_k_delta_threshold = 25,
};

const BoardConfigButton BOARD_CONFIG_BUTTON = {
  .buttons = {
    [BUTTON_ID_BACK] = {"Back"},
    [BUTTON_ID_UP] = {"Up"},
    [BUTTON_ID_SELECT] = {"Select"},
    [BUTTON_ID_DOWN] = {"Down"},
  },
};

void board_early_init(void) {
}

void board_init(void) {
}
