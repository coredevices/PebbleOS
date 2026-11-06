/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>
#include <pbl/board/otp_layout.h>
#include <pbl/drivers/ambient/opt3001.h>
#include <pbl/drivers/audio/da7212.h>
#include <pbl/drivers/audio/nrf5.h>
#include <pbl/drivers/backlight/pwm.h>
#include <pbl/drivers/display/sharp_ls013b7dh01.h>
#include <pbl/drivers/flash/spi_nor.h>
#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/i2c/nrf5.h>
#include <pbl/drivers/imu/lsm6dso/lsm6dso.h>
#include <pbl/drivers/imu/mmc5603nj/mmc5603nj.h>
#include <pbl/drivers/input/gpio_keys.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/drivers/pwm/nrf5.h>
#include <pbl/drivers/pwm/nrf5_rtc.h>
#include <pbl/drivers/regulator/npm1300.h>
#include <pbl/drivers/rtc/retained.h>
#include <pbl/drivers/spi/nrf5.h>
#include <pbl/drivers/spi_mem/nrf5_qspi.h>
#include <pbl/drivers/temperature/nrf5.h>
#include <pbl/drivers/uart/nrf5.h>
#include <pbl/drivers/vibe/drv2604.h>
#include <pbl/input/input.h>
#include <pbl/kernel/irq.h>
#include <pbl/util/size.h>

#include <board/otp_layout.h>
#include <hal/nrf_gpio.h>

PBL_SPI_MEM_NRF5_QSPI_DEFINE(s_qspi, "qspi", 8000000, PBL_GPIO(NRF5_GPIO_P0, 19, 0),
                             PBL_GPIO(NRF5_GPIO_P0, 17, 0), PBL_GPIO(NRF5_GPIO_P0, 20, 0),
                             PBL_GPIO(NRF5_GPIO_P0, 21, 0), PBL_GPIO(NRF5_GPIO_P0, 22, 0),
                             PBL_GPIO(NRF5_GPIO_P0, 23, 0));
PBL_IRQ_CONNECT(QSPI, 5, pbl_spi_mem_nrf5_qspi_irq_handler, &s_qspi, 0);

static const struct pbl_flash_otp_cell s_otp_cells[] = {BOARD_OTP_CELLS};
static const struct pbl_flash_otp_layout s_otp_layout = {s_otp_cells, ARRAY_LENGTH(s_otp_cells)};
PBL_FLASH_SPI_NOR_DEFINE(s_flash, "flash", &s_qspi.mem, &pbl_flash_nor_gd25lq255e, &s_otp_layout);

const struct pbl_flash *const FLASH = &s_flash.flash;
/* PERIPHERAL ID 43 */

PBL_UART_NRF5_DEFINE(s_uart0, "uart0", 0, 1000000, PBL_GPIO(NRF5_GPIO_P0, 27, 0),
                     PBL_GPIO(NRF5_GPIO_P0, 5, 0), 2, nullptr);
const struct pbl_uart *const CONSOLE_UART = &s_uart0.uart;
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

// Tinted rounded corners on the glass
static const uint8_t s_display_corners[] = {3, 1, 1};

// EXTCOMIN feeds a flip-flop, generating a 60 Hz 50% VCOM
PBL_DISPLAY_SHARP_LS013B7DH01_DEFINE(
    s_display, "display",
    PBL_SPI_DEV(&s_spi3.bus, 0, 1000000, PBL_SPI_MODE_0 | PBL_SPI_CS_HIGH | PBL_SPI_LSB_FIRST),
    PBL_GPIO(NRF5_GPIO_P0, 4, 0), PBL_PWM(&s_extcomin.ctrl, 0, PBL_PWM_HZ(120), 0), false,
    .corners = s_display_corners, .num_corners = ARRAY_LENGTH(s_display_corners));

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
// Codec and microphone supply
PBL_NPM1300_REGULATOR_DEFINE(s_npm1300_ldsw1, "npm1300_ldsw1", &s_npm1300, PBL_NPM1300_LDSW1, 1800,
                             true, true);
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

PBL_AUDIO_DA7212_DEFINE(s_da7212, "da7212", &s_i2c_iic2.bus, 0x1A, &s_npm1300_ldsw1.reg);

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

/* PERIPHERAL ID 11 */

PBL_AUDIO_NRF5_PDM_DEFINE(s_mic, "mic", PBL_GPIO(NRF5_GPIO_P1, 0, 0), PBL_GPIO(NRF5_GPIO_P0, 24, 0),
                          0);
PBL_IRQ_CONNECT(PDM, 7, pbl_audio_nrf5_pdm_irq_handler, &s_mic, 0);

const struct pbl_audio *const MIC = &s_mic.audio;

// DA7212 line output, the codec clocked from the I2S master clock and driving BCLK/WCLK
static const struct pbl_audio_link s_speaker_link = {
  .codec = &s_da7212.codec,
  .fmt = PBL_AUDIO_FMT_I2S | PBL_AUDIO_FMT_CODEC_MASTER,
};

PBL_AUDIO_NRF5_I2S_DEFINE(s_speaker, "speaker", &s_speaker_link,
                          .sck = PBL_GPIO(NRF5_GPIO_P0, 12, 0),
                          .lrck = PBL_GPIO(NRF5_GPIO_P0, 7, 0), .mck = PBL_GPIO(NRF5_GPIO_P1, 9, 0),
                          .sdout = PBL_GPIO(NRF5_GPIO_P0, 13, 0));
PBL_IRQ_CONNECT(I2S, 5, pbl_audio_nrf5_i2s_irq_handler, &s_speaker, 0);

const struct pbl_audio *const SPEAKER = &s_speaker.audio;

/* sensor SPI bus */

/* asterix shares SPI with flash, which we don't support */

PBL_PWM_NRF5_DEFINE(s_pwm0, "pwm0", 0, nullptr, PBL_GPIO(NRF5_GPIO_P0, 26, 0));

PBL_BACKLIGHT_PWM_DEFINE(s_backlight, .enable = PBL_GPIO(NRF5_GPIO_P1, 8, 0),
                         .pwm = PBL_PWM(&s_pwm0.ctrl, 0, PBL_PWM_HZ(256), 0),
                         .max_duty_cycle_percent = 67);

const struct pbl_backlight *const BACKLIGHT = &s_backlight.backlight;

const Npm1300Config NPM1300_CONFIG = {
  // 128mA = ~1C (rapid charge)
  .chg_current_ma = 128,    .dischg_limit_ma = 200, .term_current_pct = 10,  .vterm_mv = 4200,
  .vterm_reduced_mv = 4000, .ntc_kohm = 10,         .thermistor_beta = 3380, .ntc_hot_celsius = 45,
};

const struct pbl_temp *const DIE_TEMP = NRF5_TEMP;

const struct pbl_rtc *const RTC = RETAINED_RTC;
