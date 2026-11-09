/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>
#include <pbl/drivers/ambient/qemu.h>
#include <pbl/drivers/audio/qemu.h>
#include <pbl/drivers/imu/qemu.h>
#include <pbl/drivers/rtc/qemu.h>
#include <pbl/drivers/temperature/qemu.h>
#include <pbl/drivers/vibe/qemu.h>

#include <devicetree/devices.h>

const struct pbl_uart *const CONSOLE_UART = &dt_uart2;

const struct pbl_uart *const QEMU_UART = &dt_uart1;

#ifdef CONFIG_BT_HCI_UART
const struct pbl_uart *const BT_HCI_UART = &dt_uart3;
#endif

const struct pbl_display *const DISPLAY = &dt_display;

// Microphone fed by the host
PBL_AUDIO_HOST_MIC_DEFINE(s_host_mic, "host_mic");
PBL_AUDIO_CARD_DEFINE(s_mic, "mic", &s_host_mic.dai, nullptr, nullptr, 0, 0);

const struct pbl_audio *const MIC = &s_mic;

#ifdef CONFIG_ACCEL
const struct pbl_accel *const ACCEL = QEMU_ACCEL;
#endif

const struct pbl_als *const ALS = QEMU_ALS;

const struct pbl_backlight *const BACKLIGHT = &dt_display_backlight;

#ifdef CONFIG_VIBE
const struct pbl_vibe *const VIBE = QEMU_VIBE;
#endif

const struct pbl_temp *const DIE_TEMP = QEMU_TEMP;

const struct pbl_rtc *const RTC = QEMU_RTC;

const struct pbl_touch *const TOUCH = &dt_touchscreen;
const struct pbl_flash *const FLASH = &dt_flash0;
