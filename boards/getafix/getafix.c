/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>
#include <pbl/drivers/ambient/w1160.h>
#include <pbl/drivers/audio/sf32lb52.h>
#include <pbl/drivers/backlight/aw9364e.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/display/sf32lb_lcdc.h>
#include <pbl/drivers/dma/sf32lb52.h>
#include <pbl/drivers/gpio/sf32lb.h>
#include <pbl/drivers/input/gpio_keys.h>
#include <pbl/drivers/mbox/sf32lb52.h>
#include <pbl/drivers/panel/jdi_mip.h>
#include <pbl/drivers/panel/ls013b7dd02.h>
#include <pbl/drivers/pinctrl/sf32lb52.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/drivers/pwm/sf32lb_lptim.h>
#include <pbl/drivers/remoteproc/sf32lb52.h>
#include <pbl/drivers/reset/sf32lb.h>
#include <pbl/drivers/rtc/sf32lb.h>
#include <pbl/drivers/sf32lb52/hxt.h>
#include <pbl/drivers/temperature/sf32lb.h>
#include <pbl/drivers/touch/cst816.h>
#include <pbl/drivers/uart/sf32lb.h>
#include <pbl/drivers/vibe/aw86225.h>
#include <pbl/drivers/vibe/aw8623x.h>
#include <pbl/input/input.h>
#include <pbl/kernel/irq.h>

#include <devicetree/devices.h>
#include <hardware/sf32lb52_memorymap.h>

PBL_PINCTRL_STATE_DEFINE(s_dbg_uart_default, SF32LB52_PINCTRL,
                         SF32LB52_PA19_USART1_TXD | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA,
                         SF32LB52_PA18_USART1_RXD | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA | PBL_PINCTRL_SF32LB52_PULL_UP);

static const struct pbl_dma_chan s_dbg_uart_rx_dma =
    PBL_DMA_CHAN(SF32LB52_DMAC1, 0, SF32LB52_DMA_REQ_USART1_RX, SF32LB52_DMA_PL_MEDIUM);

PBL_UART_SF32LB_DMA_DEFINE(s_uart1, "uart1", 1, 1000000, &s_dbg_uart_default, &s_dbg_uart_rx_dma,
                           nullptr);
const struct pbl_uart *const CONSOLE_UART = &s_uart1.uart;

PBL_IRQ_CONNECT(USART1, 5, pbl_uart_sf32lb_irq_handler, &s_uart1, 0);
PBL_IRQ_CONNECT(DMAC1_CH1, 5, pbl_dma_sf32lb52_irq_handler, &s_dbg_uart_rx_dma, 0);

PBL_MBOX_SF32LB52_DEFINE(s_mailbox1, "mailbox1", SF32LB52_MAILBOX1_BASE, PBL_MBOX_SF32LB52_NO_IRQ,
                         PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_MAILBOX1), nullptr);
PBL_MBOX_SF32LB52_DEFINE(s_mailbox2, "mailbox2", SF32LB52_MAILBOX2_BASE, PBL_IRQN(LCPU2HCPU),
                         PBL_CLK(nullptr, 0), nullptr);
PBL_IRQ_CONNECT(LCPU2HCPU, 5, pbl_mbox_sf32lb52_irq_handler, &s_mailbox2, 0);

#ifdef CONFIG_REMOTEPROC_SF32LB52
PBL_RPROC_SF32LB52_DEFINE(s_lcpu, "lcpu", PBL_MBOX_CHAN(&s_mailbox1.ctrl, 0),
                          PBL_MBOX_CHAN(&s_mailbox2.ctrl, 0),
                          PBL_MEM_REGION(SF32LB52_HPSYS_MBOX_BUF_BASE, SF32LB52_MBOX_BUF_SIZE));
const struct pbl_rproc_sf32lb52 *const LCPU = &s_lcpu;
#endif // CONFIG_REMOTEPROC_SF32LB52

#ifdef NIMBLE_HCI_SF32LB52_TRACE_BINARY
PBL_PINCTRL_STATE_DEFINE(s_hci_trace_uart_default, SF32LB52_PINCTRL,
                         SF32LB52_PA27_USART3_TXD | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA);

PBL_UART_SF32LB_DEFINE(s_uart3, "uart3", 3, 1000000, &s_hci_trace_uart_default, nullptr);
const struct pbl_uart *const HCI_TRACE_UART = &s_uart3.uart;
#endif // NIMBLE_HCI_SF32LB52_TRACE_BINARY

PBL_SF32LB52_HXT_DEFINE(s_hxt, "hxt", &dt_flash0, "soc-config-user", "soc-config-cust");

const struct pbl_flash *const FLASH = &dt_flash0;

PBL_PINCTRL_STATE_DEFINE(
    s_display_default, SF32LB52_PINCTRL,
    SF32LB52_PA40_LCDC1_JDI_XRST | PBL_PINCTRL_SF32LB52_INPUT | PBL_PINCTRL_SF32LB52_DRIVE_4MA,
    SF32LB52_PA08_LCDC1_JDI_VST | PBL_PINCTRL_SF32LB52_INPUT | PBL_PINCTRL_SF32LB52_DRIVE_8MA,
    SF32LB52_PA39_LCDC1_JDI_VCK | PBL_PINCTRL_SF32LB52_INPUT | PBL_PINCTRL_SF32LB52_DRIVE_4MA,
    SF32LB52_PA07_LCDC1_JDI_ENB | PBL_PINCTRL_SF32LB52_INPUT | PBL_PINCTRL_SF32LB52_DRIVE_8MA,
    SF32LB52_PA06_LCDC1_JDI_HST | PBL_PINCTRL_SF32LB52_INPUT | PBL_PINCTRL_SF32LB52_DRIVE_8MA,
    SF32LB52_PA41_LCDC1_JDI_HCK | PBL_PINCTRL_SF32LB52_INPUT | PBL_PINCTRL_SF32LB52_DRIVE_4MA,
    SF32LB52_PA05_LCDC1_JDI_R1 | PBL_PINCTRL_SF32LB52_INPUT | PBL_PINCTRL_SF32LB52_DRIVE_8MA,
    SF32LB52_PA42_LCDC1_JDI_R2 | PBL_PINCTRL_SF32LB52_INPUT | PBL_PINCTRL_SF32LB52_DRIVE_4MA,
    SF32LB52_PA04_LCDC1_JDI_G1 | PBL_PINCTRL_SF32LB52_INPUT | PBL_PINCTRL_SF32LB52_DRIVE_8MA,
    SF32LB52_PA43_LCDC1_JDI_G2 | PBL_PINCTRL_SF32LB52_INPUT | PBL_PINCTRL_SF32LB52_DRIVE_8MA,
    SF32LB52_PA03_LCDC1_JDI_B1 | PBL_PINCTRL_SF32LB52_INPUT | PBL_PINCTRL_SF32LB52_DRIVE_8MA,
    SF32LB52_PA02_LCDC1_JDI_B2 | PBL_PINCTRL_SF32LB52_INPUT | PBL_PINCTRL_SF32LB52_DRIVE_8MA);

PBL_PINCTRL_STATE_DEFINE(s_lptim2_default, SF32LB52_PINCTRL,
                         SF32LB52_PA24_PBR_LPTIM2_OUT | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA,
                         SF32LB52_PA25_PBR_LPTIM2_INV_OUT | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA);
PBL_PWM_SF32LB_LPTIM_DEFINE(s_lptim2, "lptim2", 2, &s_lptim2_default, nullptr);

static const struct pbl_gpio s_display_discharge[] = {
  PBL_GPIO(SF32LB_GPIO1, 3, 0),  PBL_GPIO(SF32LB_GPIO1, 39, 0), PBL_GPIO(SF32LB_GPIO1, 40, 0),
  PBL_GPIO(SF32LB_GPIO1, 41, 0), PBL_GPIO(SF32LB_GPIO1, 42, 0),
};

static const struct pbl_panel_timing s_panel_timing = {
  .clock_hz = 746268,
  .hactive = 260,
  .hback_porch = 0,
  .hfront_porch = 8,
  .vactive = 260,
  .vback_porch = 0,
  .vfront_porch = 4,
  .enable_start = 6,
  .enable_end = 128,
};

PBL_PANEL_JDI_MIP_DEFINE(s_panel, "panel", &s_panel_timing, PBL_GPIO(SF32LB_GPIO1, 0, 0),
                         PBL_GPIO(SF32LB_GPIO1, 9, 0),
                         PBL_PWM(&s_lptim2.ctrl, 0, PBL_PWM_HZ(60), 0));

PBL_DISPLAY_SF32LB_LCDC_DEFINE(s_display, "display", SF32LB52_LCDC1_BASE, PBL_IRQN(LCDC1),
                               PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_LCDC1),
                               PBL_RESET(SF32LB_HPSYS_RCC_RESET, SF32LB52_HPSYS_RST_LCDC1),
                               &s_panel.panel, 260, 260, true,
                               PBL_PINCTRL(&s_display_default, nullptr, 0), s_display_discharge,
                               ARRAY_LENGTH(s_display_discharge),
                               .round_mask = pbl_panel_ls013b7dd02_round_mask);

const struct pbl_display *const DISPLAY = &s_display.disp;
PBL_IRQ_CONNECT(LCDC1, 5, pbl_display_sf32lb_lcdc_irq_handler, &s_display, 0);

PBL_AW9364E_DEFINE(s_aw9364e, PBL_GPIO(SF32LB_GPIO1, 1, 0));

const struct pbl_backlight *const BACKLIGHT = &s_aw9364e.backlight;

const struct pbl_accel *const ACCEL = &dt_lis2dw12;

const struct pbl_mag *const MAG = &dt_mmc5603nj;

PBL_CST816_DEFINE(s_cst816, &dt_i2c2, 0x15, 0x6A, PBL_GPIO(SF32LB_GPIO1, 38, PBL_GPIO_PULL_UP),
                  PBL_GPIO(SF32LB_GPIO1, 28, PBL_GPIO_ACTIVE_LOW), .size_x = 260, .size_y = 260);

const struct pbl_touch *const TOUCH = &s_cst816.touch;

const struct pbl_npm1300_charger *const NPM1300_CHARGER = &dt_pmic_charger;

// Bench-calibrated on 1 DVT2 unit, fit above 4500 lux; readings below
// ~4000 lux deviated from the fit on that unit and need a re-measure.
static const struct pbl_als_scale s_w1160_scale = {.offset = 0, .num = 50, .den = 517};

PBL_W1160_DEFINE(s_w1160, &dt_i2c3, 0x48, &s_w1160_scale);

const struct pbl_als *const ALS = &s_w1160.als;

#ifdef CONFIG_BOARD_GETAFIX_DVT2
PBL_AW86225_DEFINE(s_aw86225, &dt_i2c3, 0x58, PBL_GPIO(SF32LB_GPIO1, 20, PBL_GPIO_ACTIVE_LOW));

const struct pbl_vibe *const VIBE = &s_aw86225.vibe;
#else
PBL_AW8623X_DEFINE(s_aw8623x, &dt_i2c3, 0x5a, PBL_GPIO(SF32LB_GPIO1, 20, PBL_GPIO_ACTIVE_LOW));

const struct pbl_vibe *const VIBE = &s_aw8623x.vibe;
#endif

PBL_GPIO_KEYS_DEFINE(
    BUTTONS, "buttons",
    PBL_GPIO_KEYS_KEY("Back", PBL_GPIO(SF32LB_GPIO1, 34, 0), PBL_INPUT_KEY_BACK, 40),
    PBL_GPIO_KEYS_KEY("Up", PBL_GPIO(SF32LB_GPIO1, 35, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_UP, 40),
    PBL_GPIO_KEYS_KEY("Select", PBL_GPIO(SF32LB_GPIO1, 36, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_SELECT, 40),
    PBL_GPIO_KEYS_KEY("Down", PBL_GPIO(SF32LB_GPIO1, 37, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_DOWN, 40));

static const struct pbl_dma_chan s_mic_dma =
    PBL_DMA_CHAN(SF32LB52_DMAC1, 4, SF32LB52_DMA_REQ_PDM1_L, SF32LB52_DMA_PL_LOW);
PBL_PINCTRL_STATE_DEFINE(s_mic_default, SF32LB52_PINCTRL,
                         SF32LB52_PA22_PDM1_CLK | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA,
                         SF32LB52_PA23_PDM1_DATA | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA | PBL_PINCTRL_SF32LB52_PULL_DOWN);
PBL_AUDIO_SF32LB52_PDM_DEFINE(s_mic, "mic", PBL_PINCTRL(&s_mic_default, nullptr, 0), PBL_IRQN(PDM1),
                              &s_mic_dma, PBL_IRQN(DMAC1_CH5), &dt_ldsw2);
PBL_IRQ_CONNECT(PDM1, 5, pbl_audio_sf32lb52_pdm_irq_handler, &s_mic, 0);
PBL_IRQ_CONNECT(DMAC1_CH5, 5, pbl_dma_sf32lb52_irq_handler, &s_mic_dma, 0);

const struct pbl_audio *const MIC = &s_mic.audio;

const struct pbl_temp *const DIE_TEMP = SF32LB_TEMP;

const struct pbl_rtc *const RTC = SF32LB_RTC;
