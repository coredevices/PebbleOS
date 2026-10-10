/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/audio.h>
#include <pbl/drivers/flash/qspi_flash_definitions.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/i2c/nrf5.h>
#include <pbl/drivers/mic.h>
#include <pbl/drivers/mic/nrf5/pdm_definitions.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/drivers/qspi_definitions.h>
#include <pbl/drivers/rtc.h>
#include <pbl/drivers/speaker/nrf5/da7212_definitions.h>
#include <pbl/drivers/uart/nrf5.h>
#include <pbl/kernel/irq.h>
#include <pbl/logging/logging.h>

#include <board/board.h>
#include <flash_region/flash_region.h>
#include <nrfx_i2s.h>

// QSPI
#include <hal/nrf_clock.h>
#include <hal/nrf_gpio.h>
#include <nrfx_gpiote.h>
#include <nrfx_pdm.h>
#include <nrfx_twim.h>

static QSPIPortState s_qspi_port_state;
static QSPIPort QSPI_PORT = {
  .state = &s_qspi_port_state,
  .clk_freq_hz = 8000000UL,
  .cs_gpio = NRF_GPIO_PIN_MAP(0, 17),
  .clk_gpio = NRF_GPIO_PIN_MAP(0, 19),
  .data_gpio = {
    NRF_GPIO_PIN_MAP(0, 20),
    NRF_GPIO_PIN_MAP(0, 21),
    NRF_GPIO_PIN_MAP(0, 22),
    NRF_GPIO_PIN_MAP(0, 23),
  },
};
QSPIPort *const QSPI = &QSPI_PORT;

static QSPIFlashState s_qspi_flash_state;
static QSPIFlash QSPI_FLASH_DEVICE = {
  .state = &s_qspi_flash_state,
  .qspi = &QSPI_PORT,
  .read_mode = QSPI_FLASH_READ_READ4IO,
  .write_mode = QSPI_FLASH_WRITE_PP4O,
};
QSPIFlash *const QSPI_FLASH = &QSPI_FLASH_DEVICE;
/* PERIPHERAL ID 43 */

static UARTDeviceState s_dbg_uart_state;
static UARTDevice DBG_UART_DEVICE = {
  .state = &s_dbg_uart_state,
  .tx_gpio = NRF_GPIO_PIN_MAP(0, 27),
  .rx_gpio = NRF_GPIO_PIN_MAP(0, 5),
  .rts_gpio = NRF_UARTE_PSEL_DISCONNECTED,
  .cts_gpio = NRF_UARTE_PSEL_DISCONNECTED,
  .periph = NRFX_UARTE_INSTANCE(0),
  .counter = NRFX_TIMER_INSTANCE(2),
};
UARTDevice *const DBG_UART = &DBG_UART_DEVICE;
PBL_IRQ_CONNECT(UART0_UARTE0, NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY, nrfx_uarte_0_irq_handler, ,
                0);
/* PERIPHERAL ID 8 */

/* buttons */
PBL_IRQ_CONNECT(TIMER1, NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY, nrfx_timer_1_irq_handler, , 0);
PBL_IRQ_CONNECT(TIMER2, NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY, nrfx_timer_2_irq_handler, , 0);

/* display */
PwmState DISPLAY_EXTCOMIN_STATE;
PBL_IRQ_CONNECT(SPIM3, NRFX_SPIM_DEFAULT_CONFIG_IRQ_PRIORITY, nrfx_spim_3_irq_handler, , 0);

/* PERIPHERAL ID 10 */

/* EXTI */
PBL_IRQ_CONNECT(GPIOTE, NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY, nrfx_gpiote_0_irq_handler, , 0);

/* nPM1300 */
PBL_I2C_NRF5_DEFINE(s_i2c_npmc_iic1, "i2c_npmc_iic1", 1, NRF_TWIM_FREQ_400K,
                    PBL_GPIO(NRF5_GPIO_P0, 14, 0), PBL_GPIO(NRF5_GPIO_P0, 15, 0), NULL);
PBL_IRQ_CONNECT(SPI1_SPIM1_SPIS1_TWI1_TWIM1_TWIS1, NRFX_TWIM_DEFAULT_CONFIG_IRQ_PRIORITY,
                nrfx_twim_1_irq_handler, , 0);
/* PERIPHERAL ID 9 */

PBL_NPM1300_DEFINE(s_npm1300, &s_i2c_npmc_iic1.bus, 0x6B, &NPM1300_CONFIG,
                   {NRFX_GPIOTE_INSTANCE(0), 1, NRF_GPIO_PIN_MAP(1, 12)});

const struct pbl_npm1300 *const NPM1300 = &s_npm1300;

PBL_NPM1300_REGULATOR_DEFINE(s_npm1300_buck1, "npm1300_buck1", &s_npm1300, PBL_NPM1300_BUCK1, 1800,
                             false, true);
PBL_NPM1300_REGULATOR_DEFINE(s_npm1300_buck2, "npm1300_buck2", &s_npm1300, PBL_NPM1300_BUCK2, 3000,
                             false, true);
PBL_NPM1300_REGULATOR_DEFINE(s_npm1300_ldsw2, "npm1300_ldsw2", &s_npm1300, PBL_NPM1300_LDSW2, 1800,
                             true, true);

/* peripheral I2C bus */
PBL_I2C_NRF5_DEFINE(s_i2c_iic2, "i2c_iic2", 0, NRF_TWIM_FREQ_400K, PBL_GPIO(NRF5_GPIO_P0, 25, 0),
                    PBL_GPIO(NRF5_GPIO_P0, 11, 0), NULL);
PBL_IRQ_CONNECT(SPI0_SPIM0_SPIS0_TWI0_TWIM0_TWIS0, NRFX_TWIM_DEFAULT_CONFIG_IRQ_PRIORITY,
                nrfx_twim_0_irq_handler, , 0);

static const struct pbl_i2c_dev I2C_SLAVE_DRV2604 = PBL_I2C_DEV(&s_i2c_iic2.bus, 0x5A);

const struct pbl_i2c_dev *const I2C_DRV2604 = &I2C_SLAVE_DRV2604;

static const struct pbl_i2c_dev I2C_SLAVE_OPT3001 = PBL_I2C_DEV(&s_i2c_iic2.bus, 0x44);

const struct pbl_i2c_dev *const I2C_OPT3001 = &I2C_SLAVE_OPT3001;

static const struct pbl_i2c_dev I2C_SLAVE_DA7212 = PBL_I2C_DEV(&s_i2c_iic2.bus, 0x1A);

const struct pbl_i2c_dev *const I2C_DA7212 = &I2C_SLAVE_DA7212;

static const struct pbl_i2c_dev I2C_SLAVE_MMC5603NJ = PBL_I2C_DEV(&s_i2c_iic2.bus, 0x30);

const struct pbl_i2c_dev *const I2C_MMC5603NJ = &I2C_SLAVE_MMC5603NJ;

static const struct pbl_i2c_dev I2C_SLAVE_BMP390 = PBL_I2C_DEV(&s_i2c_iic2.bus, 0x76);

const struct pbl_i2c_dev *const I2C_BMP390 = &I2C_SLAVE_BMP390;

static LSM6DSOState s_lsm6dso_state;

static const LSM6DSOConfig s_lsm6dso_config = {
  .state = &s_lsm6dso_state,
  .i2c = PBL_I2C_DEV(&s_i2c_iic2.bus, 0x6A),
  .int1 =
      {
        .peripheral = NRFX_GPIOTE_INSTANCE(0),
        .channel = 7,
        .gpio_pin = NRF_GPIO_PIN_MAP(1, 13),
      },
  .int1_in = PBL_GPIO(NRF5_GPIO_P1, 13, 0),
  .axis_map =
      {
        [AXIS_X] = 1,
        [AXIS_Y] = 0,
        [AXIS_Z] = 2,
      },
  .axis_dir = {
    [AXIS_X] = 1,
    [AXIS_Y] = 1,
    [AXIS_Z] = 1,
  },
};

const LSM6DSOConfig *const LSM6DSO = &s_lsm6dso_config;

PBL_IRQ_CONNECT(I2S, 5, nrfx_i2s_0_irq_handler, , 0);

PBL_IRQ_CONNECT(PDM, NRFX_PDM_DEFAULT_CONFIG_IRQ_PRIORITY, NRFX_PDM_INST_HANDLER_GET(0), , 0);

/* PERIPHERAL ID 11 */

/* Microphone */
static MicDeviceState s_mic_state_storage;
static MicDevice s_mic_device = {
  .state = &s_mic_state_storage,
  .pdm_instance = NRFX_PDM_INSTANCE(0),
  .clk_pin = NRF_GPIO_PIN_MAP(1, 0),   // P1.00 - PDM CLK
  .data_pin = NRF_GPIO_PIN_MAP(0, 24), // P0.24 - PDM DATA
  .channels = 1,
};
MicDevice *const MIC = &s_mic_device;

/* Speaker / audio output (DA7212 codec over I2S) */
static AudioDeviceState s_audio_state_storage;
static void prv_audio_power_up(void) {
  pbl_npm1300_set_dischg_limit_ma(NPM1300, NPM1300_DISCHG_LIMIT_MA_MAX);
}
static void prv_audio_power_down(void) {
  pbl_npm1300_set_dischg_limit_ma(NPM1300, NPM1300_CONFIG.dischg_limit_ma);
}
static const BoardPowerOps s_audio_power_ops = {
  .power_up = prv_audio_power_up,
  .power_down = prv_audio_power_down,
};
static const AudioDevice s_audio_device = {
  .state = &s_audio_state_storage,
  .i2s_instance = NRFX_I2S_INSTANCE(0),
  .sck_pin = NRF_GPIO_PIN_MAP(0, 12),    // P0.12 - I2S SCK  -> DA7212 BCLK
  .lrck_pin = NRF_GPIO_PIN_MAP(0, 7),    // P0.07 - I2S LRCK -> DA7212 WCLK
  .mck_pin = NRF_GPIO_PIN_MAP(1, 9),     // P1.09 - I2S MCK  -> DA7212 MCLK
  .sdout_pin = NRF_GPIO_PIN_MAP(0, 13),  // P0.13 - I2S SDOUT -> DA7212 DATA_IN
  .sdin_pin = NRF_I2S_PIN_NOT_CONNECTED, // codec DATA_OUT unused for playback
  .irq_priority = 5,
  .codec = &I2C_SLAVE_DA7212,
  .power_ops = &s_audio_power_ops,
  .samplerate = 16000,
};
AudioDevice *const AUDIO = (AudioDevice *)&s_audio_device;

/* sensor SPI bus */

/* asterix shares SPI with flash, which we don't support */

PwmState BACKLIGHT_PWM_STATE;
PBL_IRQ_CONNECT(PWM0, NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY, nrfx_pwm_0_irq_handler, , 0);

PBL_IRQ_CONNECT(RTC1, 7, rtc_irq_handler, , 0);

const Npm1300Config NPM1300_CONFIG = {
  // 128mA = ~1C (rapid charge)
  .chg_current_ma = 128,    .dischg_limit_ma = 200, .term_current_pct = 10,  .vterm_mv = 4200,
  .vterm_reduced_mv = 4000, .ntc_kohm = 10,         .thermistor_beta = 3380, .ntc_hot_celsius = 45,
};

void board_early_init(void) {
  PBL_LOG_ERR("asterix early init");

  NRF_NVMC->ICACHECNF |= NVMC_ICACHECNF_CACHEEN_Msk;

  nrf_clock_lf_src_set(NRF_CLOCK, NRF_CLOCK_LFCLK_XTAL);
  nrf_clock_event_clear(NRF_CLOCK, NRF_CLOCK_EVENT_LFCLKSTARTED);
  nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_LFCLKSTART);
  /* TODO: Add timeout, report failure if LFCLK does not start. For now,
   * WDT should trigger a reboot. Calibrated RC may be used as a fallback,
   * provided we can adjust BLE SCA settings at runtime.
   */
  while (!nrf_clock_event_check(NRF_CLOCK, NRF_CLOCK_EVENT_LFCLKSTARTED)) {
  }
  nrf_clock_event_clear(NRF_CLOCK, NRF_CLOCK_EVENT_LFCLKSTARTED);
}

void board_init(void) {
  uint8_t da7212_powerdown[] = {0xFD /* SYSTEM_ACTIVE */, 0};
  pbl_i2c_use(I2C_DA7212);
  pbl_i2c_write_block(I2C_DA7212, 2, da7212_powerdown);
  pbl_i2c_release(I2C_DA7212);
}
