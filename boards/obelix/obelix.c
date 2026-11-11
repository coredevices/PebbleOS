/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
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

#ifdef CONFIG_HRM
#include <pbl/drivers/hrm/gh3x2x.h>
#endif

#include <devicetree/devices.h>
#include <hardware/sf32lb52_memorymap.h>

const struct pbl_uart *const CONSOLE_UART = &dt_usart1;

const uint8_t BOARD_DISPLAY_CORNERS[] = {3, 3, 2, 1};
const uint8_t BOARD_DISPLAY_NUM_CORNERS = ARRAY_LENGTH(BOARD_DISPLAY_CORNERS);

#ifdef CONFIG_DISPLAY
const struct pbl_display *const DISPLAY = &dt_lcdc1;
#endif

#ifdef CONFIG_REMOTEPROC_SF32LB52
const struct pbl_rproc_sf32lb52 *const LCPU =
    container_of(&dt_lcpu, const struct pbl_rproc_sf32lb52, rproc);
#endif // CONFIG_REMOTEPROC_SF32LB52

#ifdef NIMBLE_HCI_SF32LB52_TRACE_BINARY
PBL_PINCTRL_STATE_DEFINE(s_hci_trace_uart_default, SF32LB52_PINCTRL,
                         SF32LB52_PA20_USART3_TXD | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA);

PBL_UART_SF32LB_DEFINE(s_usart3, "usart3", SF32LB52_USART3_BASE, PBL_SOC_IRQN_USART3,
                       PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_USART3), 1000000,
                       &s_hci_trace_uart_default);
const struct pbl_uart *const HCI_TRACE_UART = &s_usart3;
#endif // NIMBLE_HCI_SF32LB52_TRACE_BINARY

#ifdef CONFIG_FLASH
const struct pbl_flash *const FLASH = &dt_flash0;
#endif

#ifdef CONFIG_PMIC_NPM1300
const struct pbl_npm1300_charger *const NPM1300_CHARGER = &dt_pmic_charger;
#endif

#ifdef CONFIG_VIBE
const struct pbl_vibe *const VIBE = &dt_haptics;
#endif

#ifdef CONFIG_BACKLIGHT
const struct pbl_backlight *const BACKLIGHT = &dt_backlight;
#endif

#ifdef CONFIG_ACCEL_LSM6DSO
const struct pbl_accel *const ACCEL = &dt_lsm6dso;
#endif

#ifdef CONFIG_MAG_MMC5603NJ
const struct pbl_mag *const MAG = &dt_mmc5603nj;
#endif

#ifdef CONFIG_TOUCH
const struct pbl_touch *const TOUCH = &dt_touchscreen;
#endif

#ifdef CONFIG_HRM
static const struct pbl_i2c_dev s_i2c_gh3x2x = PBL_I2C_DEV(&dt_i2c4, 0x14);

static HRMDeviceState s_hrm_state;
static HRMDevice s_hrm = {
  .state = &s_hrm_state,
  .i2c = &s_i2c_gh3x2x,
  .int_irq = PBL_IRQ_SPEC(&dt_gpio1_intc, 44, PBL_IRQ_TYPE_EDGE_FALLING),
  .int_input = PBL_GPIO(&dt_gpio1, 44, 0),
  .reset = PBL_GPIO(&dt_pmic_gpio, 3, PBL_GPIO_PULL_UP),
};

HRMDevice *const HRM = &s_hrm;
#endif

#ifdef CONFIG_AMBIENT_LIGHT
const struct pbl_als *const ALS = &dt_w1160;
#endif

#ifdef CONFIG_MIC
const struct pbl_audio *const MIC = &dt_mic;
#endif

#ifdef CONFIG_SPEAKER
const struct pbl_audio *const SPEAKER = &dt_speaker;
#endif

#ifdef CONFIG_TEMPERATURE
const struct pbl_temp *const DIE_TEMP = SF32LB_TEMP;
#endif

#ifdef CONFIG_RTC
const struct pbl_rtc *const RTC = SF32LB_RTC;
#endif
