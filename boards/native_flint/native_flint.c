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
#include <pbl/drivers/temp/posix.h>
#include <pbl/drivers/touch/sdl.h>
#include <pbl/drivers/uart/posix.h>
#include <pbl/drivers/vibe/posix.h>

const struct pbl_uart *const CONSOLE_UART = POSIX_CONSOLE_UART;

const struct pbl_uart *const QEMU_UART = POSIX_QEMU_UART;

#ifdef CONFIG_BT_HCI_UART
const struct pbl_uart *const BT_HCI_UART = POSIX_BT_HCI_UART;
#endif

#ifdef CONFIG_DISPLAY
const struct pbl_display *const DISPLAY = SDL_DISPLAY;
#endif

#ifdef CONFIG_MIC
const struct pbl_audio *const MIC = HOST_MIC;
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

#ifdef CONFIG_TEMP
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
