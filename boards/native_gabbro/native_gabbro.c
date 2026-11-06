/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>
#include <pbl/drivers/ambient/posix.h>
#include <pbl/drivers/audio/qemu.h>
#include <pbl/drivers/backlight/posix.h>
#include <pbl/drivers/display/sdl.h>
#include <pbl/drivers/flash/posix.h>
#include <pbl/drivers/imu/posix.h>
#include <pbl/drivers/panel/ls013b7dd02.h>
#include <pbl/drivers/rtc/posix.h>
#include <pbl/drivers/temperature/posix.h>
#include <pbl/drivers/touch/sdl.h>
#include <pbl/drivers/uart/posix.h>
#include <pbl/drivers/vibe/posix.h>

PBL_UART_POSIX_DEFINE(s_dbg_uart, "console", PBL_UART_POSIX_CONSOLE, 115200);
const struct pbl_uart *const CONSOLE_UART = &s_dbg_uart.uart;

PBL_UART_POSIX_DEFINE(s_qemu_uart, "qemu", PBL_UART_POSIX_QEMU, 115200);
const struct pbl_uart *const QEMU_UART = &s_qemu_uart.uart;

#ifdef CONFIG_BT_HCI_UART
PBL_UART_POSIX_DEFINE(s_bt_hci_uart, "bt_hci", PBL_UART_POSIX_BT_HCI, 115200);
const struct pbl_uart *const BT_HCI_UART = &s_bt_hci_uart.uart;
#endif

PBL_DISPLAY_SDL_DEFINE(s_display, "display", CONFIG_DISPLAY_SDL_WIDTH, CONFIG_DISPLAY_SDL_HEIGHT,
                       PBL_DISPLAY_FORMAT_ARGB2222, .round_mask = pbl_panel_ls013b7dd02_round_mask);

const struct pbl_display *const DISPLAY = &s_display;

PBL_AUDIO_HOST_MIC_DEFINE(s_mic, "mic");

const struct pbl_audio *const MIC = &s_mic.audio;

const struct pbl_accel *const ACCEL = POSIX_ACCEL;

const struct pbl_als *const ALS = POSIX_ALS;

const struct pbl_backlight *const BACKLIGHT = POSIX_BACKLIGHT;

const struct pbl_vibe *const VIBE = POSIX_VIBE;

const struct pbl_temp *const DIE_TEMP = POSIX_TEMP;

const struct pbl_rtc *const RTC = POSIX_RTC;

#ifdef CONFIG_TOUCH_SDL
const struct pbl_touch *const TOUCH = SDL_TOUCH;
#endif
const struct pbl_flash *const FLASH = POSIX_FLASH;
