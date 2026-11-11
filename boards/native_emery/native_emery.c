/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>
#include <pbl/drivers/als/posix.h>
#include <pbl/drivers/audio/qemu.h>
#include <pbl/drivers/backlight/posix.h>
#include <pbl/drivers/display/sdl.h>
#include <pbl/drivers/flash/posix.h>
#include <pbl/drivers/imu/posix.h>
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

#ifdef CONFIG_DISPLAY
PBL_DISPLAY_SDL_DEFINE(s_display, "display", CONFIG_DISPLAY_SDL_WIDTH, CONFIG_DISPLAY_SDL_HEIGHT,
                       PBL_DISPLAY_FORMAT_ARGB2222, .round_mask = nullptr);

const struct pbl_display *const DISPLAY = &s_display;
#endif

#ifdef CONFIG_MIC
PBL_AUDIO_HOST_MIC_DEFINE(s_host_mic, "host_mic");
PBL_AUDIO_CARD_DEFINE(s_mic, "mic", &s_host_mic.dai, nullptr, nullptr, 0, 0);

const struct pbl_audio *const MIC = &s_mic;
#endif

#ifdef CONFIG_ACCEL
const struct pbl_accel *const ACCEL = POSIX_ACCEL;
#endif

#ifdef CONFIG_ALS
const struct pbl_als *const ALS = POSIX_ALS;
#endif

#ifdef CONFIG_BACKLIGHT
const struct pbl_backlight *const BACKLIGHT = POSIX_BACKLIGHT;
#endif

#ifdef CONFIG_VIBE
const struct pbl_vibe *const VIBE = POSIX_VIBE;
#endif

#ifdef CONFIG_TEMPERATURE
const struct pbl_temp *const DIE_TEMP = POSIX_TEMP;
#endif

#ifdef CONFIG_RTC
const struct pbl_rtc *const RTC = POSIX_RTC;
#endif

#ifdef CONFIG_TOUCH_SDL
const struct pbl_touch *const TOUCH = SDL_TOUCH;
#endif

#ifdef CONFIG_FLASH
const struct pbl_flash *const FLASH = POSIX_FLASH;
#endif
