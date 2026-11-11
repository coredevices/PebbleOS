/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/dma/sf32lb52.h>
#include <pbl/drivers/gpio/sf32lb.h>
#include <pbl/drivers/pinctrl/sf32lb52.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/drivers/remoteproc/sf32lb52.h>
#include <pbl/drivers/reset/sf32lb.h>
#include <pbl/drivers/rtc/sf32lb.h>
#include <pbl/drivers/temperature/sf32lb.h>
#include <pbl/drivers/uart/sf32lb.h>
#include <pbl/kernel/irq.h>

#include <devicetree/devices.h>

const struct pbl_uart *const CONSOLE_UART = &dt_usart1;

#ifdef CONFIG_REMOTEPROC_SF32LB52
const struct pbl_rproc_sf32lb52 *const LCPU =
    container_of(&dt_lcpu, const struct pbl_rproc_sf32lb52, rproc);
#endif // CONFIG_REMOTEPROC_SF32LB52

#ifdef CONFIG_FLASH
const struct pbl_flash *const FLASH = &dt_flash0;
#endif

#ifdef CONFIG_DISPLAY
const struct pbl_display *const DISPLAY = &dt_lcdc1;
#endif

#ifdef CONFIG_BACKLIGHT
const struct pbl_backlight *const BACKLIGHT = &dt_backlight;
#endif

#ifdef CONFIG_ACCEL
const struct pbl_accel *const ACCEL = &dt_lis2dw12;
#endif

#ifdef CONFIG_MAG
const struct pbl_mag *const MAG = &dt_mmc5603nj;
#endif

#ifdef CONFIG_TOUCH
const struct pbl_touch *const TOUCH = &dt_touchscreen;
#endif

#ifdef CONFIG_PMIC_NPM1300
const struct pbl_npm1300_charger *const NPM1300_CHARGER = &dt_pmic_charger;
#endif

#ifdef CONFIG_AMBIENT_LIGHT
const struct pbl_als *const ALS = &dt_w1160;
#endif

#ifdef CONFIG_VIBE
const struct pbl_vibe *const VIBE = &dt_haptics;
#endif

#ifdef CONFIG_MIC
const struct pbl_audio *const MIC = &dt_mic;
#endif

#ifdef CONFIG_TEMPERATURE
const struct pbl_temp *const DIE_TEMP = SF32LB_TEMP;
#endif

#ifdef CONFIG_RTC
const struct pbl_rtc *const RTC = SF32LB_RTC;
#endif
