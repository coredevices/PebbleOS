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
#include <hardware/sf32lb52_memorymap.h>

const struct pbl_uart *const CONSOLE_UART = &dt_usart1;

#ifdef CONFIG_REMOTEPROC_SF32LB52
const struct pbl_rproc_sf32lb52 *const LCPU =
    container_of(&dt_lcpu, const struct pbl_rproc_sf32lb52, rproc);
#endif // CONFIG_REMOTEPROC_SF32LB52

#ifdef NIMBLE_HCI_SF32LB52_TRACE_BINARY
PBL_PINCTRL_STATE_DEFINE(s_hci_trace_uart_default, SF32LB52_PINCTRL,
                         SF32LB52_PA27_USART3_TXD | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA);

PBL_UART_SF32LB_DEFINE(s_usart3, "usart3", SF32LB52_USART3_BASE, PBL_SOC_IRQN_USART3,
                       PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_USART3), 1000000,
                       &s_hci_trace_uart_default);
const struct pbl_uart *const HCI_TRACE_UART = &s_usart3;
#endif // NIMBLE_HCI_SF32LB52_TRACE_BINARY

const struct pbl_flash *const FLASH = &dt_flash0;

const struct pbl_display *const DISPLAY = &dt_lcdc1;

const struct pbl_backlight *const BACKLIGHT = &dt_backlight;

const struct pbl_accel *const ACCEL = &dt_lis2dw12;

const struct pbl_mag *const MAG = &dt_mmc5603nj;

const struct pbl_touch *const TOUCH = &dt_touchscreen;

const struct pbl_npm1300_charger *const NPM1300_CHARGER = &dt_pmic_charger;

const struct pbl_als *const ALS = &dt_w1160;

const struct pbl_vibe *const VIBE = &dt_haptics;

const struct pbl_audio *const MIC = &dt_mic;

const struct pbl_temp *const DIE_TEMP = SF32LB_TEMP;

const struct pbl_rtc *const RTC = SF32LB_RTC;
