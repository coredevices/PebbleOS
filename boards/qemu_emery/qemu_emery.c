/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>
#include <pbl/drivers/als/qemu.h>
#include <pbl/drivers/audio/qemu.h>
#include <pbl/drivers/imu/qemu.h>
#include <pbl/drivers/rtc/qemu.h>
#include <pbl/drivers/temperature/qemu.h>
#include <pbl/drivers/vibe/qemu.h>

#ifdef CONFIG_HRM
#include <pbl/drivers/hrm/qemu.h>
#endif

#include <devicetree/devices.h>

const struct pbl_uart *const CONSOLE_UART = &dt_uart2;

const struct pbl_uart *const QEMU_UART = &dt_uart1;

#ifdef CONFIG_BT_HCI_UART
const struct pbl_uart *const BT_HCI_UART = &dt_uart3;
#endif

#ifdef CONFIG_DISPLAY
const struct pbl_display *const DISPLAY = &dt_display;
#endif

#ifdef CONFIG_SPEAKER
const struct pbl_audio *const SPEAKER = &dt_speaker;
#endif

#ifdef CONFIG_MIC
// Microphone fed by the host
PBL_AUDIO_HOST_MIC_DEFINE(s_host_mic, "host_mic");
PBL_AUDIO_CARD_DEFINE(s_mic, "mic", &s_host_mic.dai, nullptr, nullptr, 0, 0);

const struct pbl_audio *const MIC = &s_mic;
#endif

#ifdef CONFIG_HRM
static HRMDeviceState s_hrm_state;
static HRMDevice s_hrm = {
  .state = &s_hrm_state,
};
HRMDevice *const HRM = &s_hrm;
#endif

#ifdef CONFIG_ACCEL
const struct pbl_accel *const ACCEL = QEMU_ACCEL;
#endif

#ifdef CONFIG_ALS
const struct pbl_als *const ALS = QEMU_ALS;
#endif

#ifdef CONFIG_BACKLIGHT
const struct pbl_backlight *const BACKLIGHT = &dt_display_backlight;
#endif

#ifdef CONFIG_VIBE
const struct pbl_vibe *const VIBE = QEMU_VIBE;
#endif

#ifdef CONFIG_TEMPERATURE
const struct pbl_temp *const DIE_TEMP = QEMU_TEMP;
#endif

#ifdef CONFIG_RTC
const struct pbl_rtc *const RTC = QEMU_RTC;
#endif

#ifdef CONFIG_TOUCH
const struct pbl_touch *const TOUCH = &dt_touchscreen;
#endif

#ifdef CONFIG_FLASH
const struct pbl_flash *const FLASH = &dt_flash0;
#endif
