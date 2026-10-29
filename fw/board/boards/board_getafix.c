/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/ambient/w1160.h>
#include <pbl/drivers/audio/sf32lb52.h>
#include <pbl/drivers/backlight/aw9364e.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/display/sf32lb_lcdc.h>
#include <pbl/drivers/dma/sf32lb52.h>
#include <pbl/drivers/imu/lis2dw12/lis2dw12.h>
#include <pbl/drivers/imu/mmc5603nj/mmc5603nj.h>
#include <pbl/drivers/input/gpio_keys.h>
#include <pbl/drivers/panel/jdi_mip.h>
#include <pbl/drivers/pinctrl/sf32lb52.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/drivers/pwm/sf32lb_lptim.h>
#include <pbl/drivers/regulator/npm1300.h>
#include <pbl/drivers/reset/sf32lb.h>
#include <pbl/drivers/temperature/sf32lb.h>
#include <pbl/drivers/touch/cst816.h>
#include <pbl/drivers/uart/sf32lb.h>
#include <pbl/drivers/vibe/aw86225.h>
#include <pbl/drivers/vibe/aw8623x.h>
#include <pbl/kernel/irq.h>

#include <board/board.h>
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
const struct pbl_uart *const DBG_UART = &s_uart1.uart;

PBL_IRQ_CONNECT(USART1, 5, pbl_uart_sf32lb_irq_handler, &s_uart1, 0);
PBL_IRQ_CONNECT(DMAC1_CH1, 5, pbl_dma_sf32lb52_irq_handler, &s_dbg_uart_rx_dma, 0);

#ifdef NIMBLE_HCI_SF32LB52_TRACE_BINARY
PBL_PINCTRL_STATE_DEFINE(s_hci_trace_uart_default, SF32LB52_PINCTRL,
                         SF32LB52_PA27_USART3_TXD | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA);

PBL_UART_SF32LB_DEFINE(s_uart3, "uart3", 3, 1000000, &s_hci_trace_uart_default, nullptr);
const struct pbl_uart *const HCI_TRACE_UART = &s_uart3.uart;
#endif // NIMBLE_HCI_SF32LB52_TRACE_BINARY

static QSPIPortState s_qspi_port_state = {
  .cfg =
      {
        .Instance = FLASH2,
        .line = HAL_FLASH_QMODE,
        .base = FLASH2_BASE_ADDR,
        .msize = 32,
        .SpiMode = SPI_MODE_NOR,
      },
  .dma =
      {
        .Instance = DMA1_Channel2,
        .dma_irq = DMAC1_CH2_IRQn,
        .request = DMA_REQUEST_1,
      },
  .t_enter_deep_us = 3,
  .t_exit_deep_us = 20,
};

static QSPIPort QSPI_PORT = {
  .state = &s_qspi_port_state,
  .clk_div = 0U,
};
QSPIPort *const QSPI = &QSPI_PORT;

static QSPIFlashState s_qspi_flash_state;
static QSPIFlash QSPI_FLASH_DEVICE = {
  .state = &s_qspi_flash_state,
  .qspi = &QSPI_PORT,
};
QSPIFlash *const QSPI_FLASH = &QSPI_FLASH_DEVICE;

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
  .hactive = PBL_DISPLAY_WIDTH,
  .hback_porch = 0,
  .hfront_porch = 8,
  .vactive = PBL_DISPLAY_HEIGHT,
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
                               &s_panel.panel, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT, true,
                               PBL_PINCTRL(&s_display_default, nullptr, 0), s_display_discharge,
                               ARRAY_LENGTH(s_display_discharge));

const struct pbl_display *const DISPLAY = &s_display.disp;
PBL_IRQ_CONNECT(LCDC1, 5, pbl_display_sf32lb_lcdc_irq_handler, &s_display, 0);

PBL_AW9364E_DEFINE(s_aw9364e, PBL_GPIO(SF32LB_GPIO1, 1, 0));

const struct pbl_backlight *const BACKLIGHT = &s_aw9364e.backlight;

PBL_PINCTRL_STATE_DEFINE(s_i2c1_default, SF32LB52_PINCTRL,
                         SF32LB52_PA32_I2C1_SCL | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA,
                         SF32LB52_PA33_I2C1_SDA | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA);
static const struct pbl_dma_chan s_i2c1_dma =
    PBL_DMA_CHAN(SF32LB52_DMAC1, 5, SF32LB52_DMA_REQ_I2C1, SF32LB52_DMA_PL_HIGH);
PBL_I2C_SF32LB_DMA_DEFINE(s_i2c1, "i2c1", 1, 400000, &s_i2c1_default, &s_i2c1_dma, nullptr);

PBL_IRQ_CONNECT(I2C1, 5, pbl_i2c_sf32lb_irq_handler, &s_i2c1, 0);

PBL_LIS2DW12_DEFINE(s_lis2dw12, &s_i2c1.bus, 0x19,
                    .int1 = PBL_IRQ_SPEC(SF32LB_GPIO1_INTC, 26, PBL_IRQ_TYPE_EDGE_RISING),
                    .int1_in = PBL_GPIO(SF32LB_GPIO1, 26, 0),
                    .axis_map = {[AXIS_X] = 0, [AXIS_Y] = 1, [AXIS_Z] = 2},
                    .axis_dir = {[AXIS_X] = -1, [AXIS_Y] = 1, [AXIS_Z] = 1});

const struct pbl_accel *const ACCEL = &s_lis2dw12.accel;

PBL_MMC5603NJ_DEFINE(s_mmc5603nj, &s_i2c1.bus, 0x30,
                     .axis_map = {[AXIS_X] = 1, [AXIS_Y] = 0, [AXIS_Z] = 2},
                     .axis_dir = {[AXIS_X] = 1, [AXIS_Y] = -1, [AXIS_Z] = 1});

const struct pbl_mag *const MAG = &s_mmc5603nj.mag;

PBL_PINCTRL_STATE_DEFINE(s_i2c2_default, SF32LB52_PINCTRL,
                         SF32LB52_PA11_I2C2_SCL | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA,
                         SF32LB52_PA10_I2C2_SDA | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA);
PBL_I2C_SF32LB_DEFINE(s_i2c2, "i2c2", 2, 400000, &s_i2c2_default, nullptr);
PBL_IRQ_CONNECT(I2C2, 5, pbl_i2c_sf32lb_irq_handler, &s_i2c2, 0);

PBL_CST816_DEFINE(s_cst816, &s_i2c2.bus, 0x15, 0x6A, PBL_GPIO(SF32LB_GPIO1, 38, PBL_GPIO_PULL_UP),
                  PBL_GPIO(SF32LB_GPIO1, 28, PBL_GPIO_ACTIVE_LOW), .size_x = PBL_DISPLAY_WIDTH,
                  .size_y = PBL_DISPLAY_HEIGHT);

const struct pbl_touch *const TOUCH = &s_cst816.touch;

PBL_PINCTRL_STATE_DEFINE(s_i2c3_default, SF32LB52_PINCTRL,
                         SF32LB52_PA31_I2C3_SCL | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA,
                         SF32LB52_PA30_I2C3_SDA | PBL_PINCTRL_SF32LB52_INPUT |
                             PBL_PINCTRL_SF32LB52_DRIVE_8MA);
PBL_I2C_SF32LB_DEFINE(s_i2c3, "i2c3", 3, 400000, &s_i2c3_default, nullptr);
PBL_IRQ_CONNECT(I2C3, 5, pbl_i2c_sf32lb_irq_handler, &s_i2c3, 0);

PBL_NPM1300_DEFINE(s_npm1300, &s_i2c3.bus, 0x6B);
PBL_NPM1300_CHARGER_DEFINE(s_npm1300_charger, &s_npm1300, &NPM1300_CONFIG,
                           PBL_IRQ_SPEC(SF32LB_GPIO1_INTC, 44, PBL_IRQ_TYPE_EDGE_RISING));

const struct pbl_npm1300_charger *const NPM1300_CHARGER = &s_npm1300_charger;

PBL_NPM1300_REGULATOR_DEFINE(s_npm1300_buck1, "npm1300_buck1", &s_npm1300, PBL_NPM1300_BUCK1, 1800,
                             false, false);
PBL_NPM1300_REGULATOR_DEFINE(s_npm1300_ldsw1, "npm1300_ldsw1", &s_npm1300, PBL_NPM1300_LDSW1, 1800,
                             true, true);
// Microphone supply
PBL_NPM1300_REGULATOR_DEFINE(s_npm1300_ldsw2, "npm1300_ldsw2", &s_npm1300, PBL_NPM1300_LDSW2, 0,
                             false, false);

// Bench-calibrated on 1 DVT2 unit, fit above 4500 lux; readings below
// ~4000 lux deviated from the fit on that unit and need a re-measure.
static const struct pbl_als_scale s_w1160_scale = {.offset = 0, .num = 50, .den = 517};

PBL_W1160_DEFINE(s_w1160, &s_i2c3.bus, 0x48, &s_w1160_scale);

const struct pbl_als *const ALS = &s_w1160.als;

#ifdef CONFIG_BOARD_GETAFIX_DVT2
PBL_AW86225_DEFINE(s_aw86225, &s_i2c3.bus, 0x58, PBL_GPIO(SF32LB_GPIO1, 20, PBL_GPIO_ACTIVE_LOW));

const struct pbl_vibe *const VIBE = &s_aw86225.vibe;
#else
PBL_AW8623X_DEFINE(s_aw8623x, &s_i2c3.bus, 0x5a, PBL_GPIO(SF32LB_GPIO1, 20, PBL_GPIO_ACTIVE_LOW));

const struct pbl_vibe *const VIBE = &s_aw8623x.vibe;
#endif

const Npm1300Config NPM1300_CONFIG = {
  // 70mA = 1C (max limit from datasheet)
  .chg_current_ma = 70,     .dischg_limit_ma = 200,
  .term_current_pct = 10,   .vterm_mv = 4450,
  .vterm_reduced_mv = 4000, .ntc_kohm = 10,
  .thermistor_beta = 3380,  .ntc_hot_celsius = 45,
  .vbus_current_lim0 = 500, .vbus_current_startup = 500,
};

const BoardConfigPower BOARD_CONFIG_POWER = {
  .low_power_threshold = 5U,
  .battery_capacity_hours = 150U,
};

const BoardConfig BOARD_CONFIG = {
  .backlight_on_percent = 25,
  .ambient_light_dark_threshold = 800,
  .ambient_k_delta_threshold = 100,
};

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
                              &s_mic_dma, PBL_IRQN(DMAC1_CH5), &s_npm1300_ldsw2.reg);
PBL_IRQ_CONNECT(PDM1, 5, pbl_audio_sf32lb52_pdm_irq_handler, &s_mic, 0);
PBL_IRQ_CONNECT(DMAC1_CH5, 5, pbl_dma_sf32lb52_irq_handler, &s_mic_dma, 0);

const struct pbl_audio *const MIC = &s_mic.audio;

uint32_t BSP_GetOtpBase(void) {
  return MPI2_MEM_BASE;
}

void board_early_init(void) {
}

void board_init(void) {
}

const struct pbl_temp *const DIE_TEMP = SF32LB_TEMP;
