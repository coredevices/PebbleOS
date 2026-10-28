/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/ambient/posix.h>
#include <pbl/drivers/backlight/posix.h>
#include <pbl/drivers/display/sdl.h>
#include <pbl/drivers/imu/posix.h>
#include <pbl/drivers/mic/qemu/mic_definitions.h>
#include <pbl/drivers/temperature/posix.h>
#include <pbl/drivers/uart/posix.h>
#include <pbl/drivers/vibe/posix.h>

#include <board/board.h>

static UARTDeviceState s_dbg_uart_state;

static const struct UARTDevice s_dbg_uart = {
  .state = &s_dbg_uart_state,
  .channel = 0,
};

UARTDevice *const DBG_UART = &s_dbg_uart;

static UARTDeviceState s_qemu_uart_state;

static const struct UARTDevice s_qemu_uart = {
  .state = &s_qemu_uart_state,
  .channel = 1,
};

UARTDevice *const QEMU_UART = &s_qemu_uart;

#ifdef CONFIG_BT_HCI_UART
static UARTDeviceState s_bt_hci_uart_state;

static const struct UARTDevice s_bt_hci_uart = {
  .state = &s_bt_hci_uart_state,
  .channel = 2,
};

UARTDevice *const BT_HCI_UART = &s_bt_hci_uart;
#endif

#if PBL_BW
#define DISPLAY_FORMAT PBL_DISPLAY_FORMAT_MONO
#else
#define DISPLAY_FORMAT PBL_DISPLAY_FORMAT_ARGB2222
#endif

PBL_DISPLAY_SDL_DEFINE(s_display, "display", PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT, DISPLAY_FORMAT);

const struct pbl_display *const DISPLAY = &s_display;

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

void board_early_init(void) {
}

void board_init(void) {
}

const struct pbl_accel *const ACCEL = POSIX_ACCEL;

const struct pbl_als *const ALS = POSIX_ALS;

const struct pbl_backlight *const BACKLIGHT = POSIX_BACKLIGHT;

const struct pbl_vibe *const VIBE = POSIX_VIBE;

const struct pbl_temp *const DIE_TEMP = POSIX_TEMP;
