/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/ambient/opt3001.h>
#include <pbl/drivers/audio.h>
#include <pbl/drivers/backlight/pwm.h>
#include <pbl/drivers/display/sharp_ls013b7dh01.h>
#include <pbl/drivers/flash/qspi_flash_definitions.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/i2c/nrf5.h>
#include <pbl/drivers/imu/lsm6dso/lsm6dso.h>
#include <pbl/drivers/imu/mmc5603nj/mmc5603nj.h>
#include <pbl/drivers/input/gpio_keys.h>
#include <pbl/drivers/mic.h>
#include <pbl/drivers/mic/nrf5/pdm_definitions.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/drivers/pwm/nrf5.h>
#include <pbl/drivers/pwm/nrf5_rtc.h>
#include <pbl/drivers/qspi_definitions.h>
#include <pbl/drivers/regulator/npm1300.h>
#include <pbl/drivers/rtc.h>
#include <pbl/drivers/speaker/nrf5/da7212_definitions.h>
#include <pbl/drivers/spi/nrf5.h>
#include <pbl/drivers/temperature/nrf5.h>
#include <pbl/drivers/uart/nrf5.h>
#include <pbl/drivers/vibe/drv2604.h>
#include <pbl/kernel/irq.h>
#include <pbl/logging/logging.h>

#include <board/board.h>
#include <flash_region/flash_region.h>
#include <nrfx_i2s.h>

// QSPI
#include <hal/nrf_clock.h>
#include <hal/nrf_gpio.h>
#include <nrfx_pdm.h>

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

PBL_UART_NRF5_DEFINE(s_uart0, "uart0", 0, 1000000, PBL_GPIO(NRF5_GPIO_P0, 27, 0),
                     PBL_GPIO(NRF5_GPIO_P0, 5, 0), 2, nullptr);
const struct pbl_uart *const DBG_UART = &s_uart0.uart;
PBL_IRQ_CONNECT(UART0_UARTE0, 7, pbl_uart_nrf5_irq_handler, &s_uart0, 0);
/* PERIPHERAL ID 8 */

/* buttons */
PBL_GPIO_KEYS_DEFINE(
    BUTTONS, "buttons",
    PBL_GPIO_KEYS_KEY("Back", PBL_GPIO(NRF5_GPIO_P0, 28, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_BACK, 40),
    PBL_GPIO_KEYS_KEY("Up", PBL_GPIO(NRF5_GPIO_P0, 29, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_UP, 40),
    PBL_GPIO_KEYS_KEY("Select", PBL_GPIO(NRF5_GPIO_P0, 30, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_SELECT, 40),
    PBL_GPIO_KEYS_KEY("Down", PBL_GPIO(NRF5_GPIO_P0, 31, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_DOWN, 40));

/* display */
PBL_PWM_NRF5_RTC_DEFINE(s_extcomin, "extcomin", 2, PBL_GPIO(NRF5_GPIO_P1, 15, 0), nullptr);
PBL_SPI_NRF5_DEFINE(s_spi3, "spi3", 3, PBL_GPIO(NRF5_GPIO_P0, 6, 0), PBL_GPIO(NRF5_GPIO_P0, 8, 0),
                    PBL_GPIO(nullptr, 0, 0), nullptr, PBL_GPIO(NRF5_GPIO_P1, 3, 0));
PBL_IRQ_CONNECT(SPIM3, 7, pbl_spi_nrf5_irq_handler, &s_spi3, 0);

// EXTCOMIN feeds a flip-flop, generating a 60 Hz 50% VCOM
PBL_DISPLAY_SHARP_LS013B7DH01_DEFINE(
    s_display, "display",
    PBL_SPI_DEV(&s_spi3.bus, 0, 1000000, PBL_SPI_MODE_0 | PBL_SPI_CS_HIGH | PBL_SPI_LSB_FIRST),
    PBL_GPIO(NRF5_GPIO_P0, 4, 0), PBL_PWM(&s_extcomin.ctrl, 0, PBL_PWM_HZ(120), 0), false);

const struct pbl_display *const DISPLAY = &s_display.disp;

/* PERIPHERAL ID 10 */

/* nPM1300 */
PBL_I2C_NRF5_DEFINE(s_i2c_npmc_iic1, "i2c_npmc_iic1", 1, 400000,
                    PBL_GPIO(NRF5_GPIO_P0, 14, PBL_GPIO_PULL_UP),
                    PBL_GPIO(NRF5_GPIO_P0, 15, PBL_GPIO_PULL_UP), nullptr);
PBL_IRQ_CONNECT(SPI1_SPIM1_SPIS1_TWI1_TWIM1_TWIS1, 7, pbl_i2c_nrf5_irq_handler, &s_i2c_npmc_iic1,
                0);
/* PERIPHERAL ID 9 */

PBL_NPM1300_DEFINE(s_npm1300, &s_i2c_npmc_iic1.bus, 0x6B);
PBL_NPM1300_CHARGER_DEFINE(s_npm1300_charger, &s_npm1300, &NPM1300_CONFIG,
                           PBL_IRQ_SPEC(NRF5_GPIO_P1_INTC, 12, PBL_IRQ_TYPE_EDGE_RISING));

const struct pbl_npm1300_charger *const NPM1300_CHARGER = &s_npm1300_charger;

PBL_NPM1300_REGULATOR_DEFINE(s_npm1300_buck1, "npm1300_buck1", &s_npm1300, PBL_NPM1300_BUCK1, 1800,
                             false, true);
PBL_NPM1300_REGULATOR_DEFINE(s_npm1300_buck2, "npm1300_buck2", &s_npm1300, PBL_NPM1300_BUCK2, 3000,
                             false, true);
PBL_NPM1300_REGULATOR_DEFINE(s_npm1300_ldsw2, "npm1300_ldsw2", &s_npm1300, PBL_NPM1300_LDSW2, 1800,
                             true, true);

/* peripheral I2C bus */
PBL_I2C_NRF5_DEFINE(s_i2c_iic2, "i2c_iic2", 0, 400000, PBL_GPIO(NRF5_GPIO_P0, 25, PBL_GPIO_PULL_UP),
                    PBL_GPIO(NRF5_GPIO_P0, 11, PBL_GPIO_PULL_UP), nullptr);
PBL_IRQ_CONNECT(SPI0_SPIM0_SPIS0_TWI0_TWIM0_TWIS0, 7, pbl_i2c_nrf5_irq_handler, &s_i2c_iic2, 0);

PBL_DRV2604_DEFINE(s_drv2604, &s_i2c_iic2.bus, 0x5A, PBL_GPIO(NRF5_GPIO_P0, 2, 0));

const struct pbl_vibe *const VIBE = &s_drv2604.vibe;

PBL_OPT3001_DEFINE(s_opt3001, &s_i2c_iic2.bus, 0x44, nullptr, .always_on = true);

const struct pbl_als *const ALS = &s_opt3001.als;

static const struct pbl_i2c_dev I2C_SLAVE_DA7212 = PBL_I2C_DEV(&s_i2c_iic2.bus, 0x1A);

const struct pbl_i2c_dev *const I2C_DA7212 = &I2C_SLAVE_DA7212;

PBL_LSM6DSO_DEFINE(s_lsm6dso, &s_i2c_iic2.bus, 0x6A,
                   .int1 = PBL_IRQ_SPEC(NRF5_GPIO_P1_INTC, 13, PBL_IRQ_TYPE_EDGE_RISING),
                   .int1_in = PBL_GPIO(NRF5_GPIO_P1, 13, 0),
                   .axis_map = {[AXIS_X] = 1, [AXIS_Y] = 0, [AXIS_Z] = 2},
                   .axis_dir = {[AXIS_X] = 1, [AXIS_Y] = 1, [AXIS_Z] = 1});

const struct pbl_accel *const ACCEL = &s_lsm6dso.accel;

PBL_MMC5603NJ_DEFINE(s_mmc5603nj, &s_i2c_iic2.bus, 0x30,
                     .axis_map = {[AXIS_X] = 1, [AXIS_Y] = 0, [AXIS_Z] = 2},
                     .axis_dir = {[AXIS_X] = 1, [AXIS_Y] = -1, [AXIS_Z] = 1});

const struct pbl_mag *const MAG = &s_mmc5603nj.mag;

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
  pbl_npm1300_charger_set_dischg_limit_ma(NPM1300_CHARGER, NPM1300_DISCHG_LIMIT_MA_MAX);
}
static void prv_audio_power_down(void) {
  pbl_npm1300_charger_set_dischg_limit_ma(NPM1300_CHARGER, NPM1300_CONFIG.dischg_limit_ma);
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

PBL_PWM_NRF5_DEFINE(s_pwm0, "pwm0", 0, nullptr, PBL_GPIO(NRF5_GPIO_P0, 26, 0));

PBL_BACKLIGHT_PWM_DEFINE(s_backlight, .enable = PBL_GPIO(NRF5_GPIO_P1, 8, 0),
                         .pwm = PBL_PWM(&s_pwm0.ctrl, 0, PBL_PWM_HZ(256), 0),
                         .max_duty_cycle_percent = 67);

const struct pbl_backlight *const BACKLIGHT = &s_backlight.backlight;

PBL_IRQ_CONNECT(RTC1, 7, rtc_irq_handler, , 0);

const Npm1300Config NPM1300_CONFIG = {
  // 128mA = ~1C (rapid charge)
  .chg_current_ma = 128,    .dischg_limit_ma = 200, .term_current_pct = 10,  .vterm_mv = 4200,
  .vterm_reduced_mv = 4000, .ntc_kohm = 10,         .thermistor_beta = 3380, .ntc_hot_celsius = 45,
};

void board_early_init(void) {
  PBL_LOG_ERR("asterix early init");

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
  pbl_i2c_write(I2C_DA7212, da7212_powerdown, 2);
}

const struct pbl_temp *const DIE_TEMP = NRF5_TEMP;
