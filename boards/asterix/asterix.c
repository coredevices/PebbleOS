/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>
#include <pbl/drivers/audio/da7212.h>
#include <pbl/drivers/audio/nrf5.h>
#include <pbl/drivers/backlight/pwm.h>
#include <pbl/drivers/display/sharp_ls013b7dh01.h>
#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/input/gpio_keys.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/drivers/pwm/nrf5.h>
#include <pbl/drivers/pwm/nrf5_rtc.h>
#include <pbl/drivers/rtc/retained.h>
#include <pbl/drivers/spi/nrf5.h>
#include <pbl/drivers/temperature/nrf5.h>
#include <pbl/drivers/uart/nrf5.h>
#include <pbl/input/input.h>
#include <pbl/kernel/irq.h>
#include <pbl/util/size.h>

#include <devicetree/devices.h>
#include <hal/nrf_gpio.h>

const struct pbl_flash *const FLASH = &dt_flash0;
/* PERIPHERAL ID 43 */

const struct pbl_uart *const CONSOLE_UART = &dt_uart0;
/* PERIPHERAL ID 8 */

/* buttons */
PBL_GPIO_KEYS_DEFINE(
    BUTTONS, "buttons",
    PBL_GPIO_KEYS_KEY("Back", PBL_GPIO(&dt_gpio0, 28, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_BACK, 40),
    PBL_GPIO_KEYS_KEY("Up", PBL_GPIO(&dt_gpio0, 29, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_UP, 40),
    PBL_GPIO_KEYS_KEY("Select", PBL_GPIO(&dt_gpio0, 30, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_SELECT, 40),
    PBL_GPIO_KEYS_KEY("Down", PBL_GPIO(&dt_gpio0, 31, PBL_GPIO_PULL_UP | PBL_GPIO_ACTIVE_LOW),
                      PBL_INPUT_KEY_DOWN, 40));

/* display */
PBL_PWM_NRF5_RTC_DEFINE(s_extcomin, "extcomin", 2, PBL_GPIO(&dt_gpio1, 15, 0), nullptr);
PBL_SPI_NRF5_DEFINE(s_spi3, "spi3", 3, PBL_GPIO(&dt_gpio0, 6, 0), PBL_GPIO(&dt_gpio0, 8, 0),
                    PBL_GPIO(nullptr, 0, 0), nullptr, PBL_GPIO(&dt_gpio1, 3, 0));
PBL_IRQ_CONNECT(SPIM3, 7, pbl_spi_nrf5_irq_handler, &s_spi3, 0);

// Tinted rounded corners on the glass
static const uint8_t s_display_corners[] = {3, 1, 1};

// EXTCOMIN feeds a flip-flop, generating a 60 Hz 50% VCOM
PBL_DISPLAY_SHARP_LS013B7DH01_DEFINE(
    s_display, "display",
    PBL_SPI_DEV(&s_spi3.bus, 0, 1000000, PBL_SPI_MODE_0 | PBL_SPI_CS_HIGH | PBL_SPI_LSB_FIRST),
    PBL_GPIO(&dt_gpio0, 4, 0), PBL_PWM(&s_extcomin.ctrl, 0, PBL_PWM_HZ(120), 0), false,
    .corners = s_display_corners, .num_corners = ARRAY_LENGTH(s_display_corners));

const struct pbl_display *const DISPLAY = &s_display.disp;

/* PERIPHERAL ID 10 */

/* nPM1300 */
/* PERIPHERAL ID 9 */

const struct pbl_npm1300_charger *const NPM1300_CHARGER = &dt_pmic_charger;

/* peripheral I2C bus */
const struct pbl_vibe *const VIBE = &dt_drv2604;

const struct pbl_als *const ALS = &dt_opt3001;

PBL_AUDIO_DA7212_DEFINE(s_da7212, "da7212", &dt_i2c0, 0x1A, &dt_ldsw1);

const struct pbl_accel *const ACCEL = &dt_lsm6dso;

const struct pbl_mag *const MAG = &dt_mmc5603nj;

/* PERIPHERAL ID 11 */

PBL_AUDIO_NRF5_PDM_DEFINE(s_mic, "mic", PBL_GPIO(&dt_gpio1, 0, 0), PBL_GPIO(&dt_gpio0, 24, 0), 0);
PBL_IRQ_CONNECT(PDM, 7, pbl_audio_nrf5_pdm_irq_handler, &s_mic, 0);

const struct pbl_audio *const MIC = &s_mic.audio;

// DA7212 line output, the codec clocked from the I2S master clock and driving BCLK/WCLK
static const struct pbl_audio_link s_speaker_link = {
  .codec = &s_da7212.codec,
  .fmt = PBL_AUDIO_FMT_I2S | PBL_AUDIO_FMT_CODEC_MASTER,
};

PBL_AUDIO_NRF5_I2S_DEFINE(s_speaker, "speaker", &s_speaker_link, .sck = PBL_GPIO(&dt_gpio0, 12, 0),
                          .lrck = PBL_GPIO(&dt_gpio0, 7, 0), .mck = PBL_GPIO(&dt_gpio1, 9, 0),
                          .sdout = PBL_GPIO(&dt_gpio0, 13, 0));
PBL_IRQ_CONNECT(I2S, 5, pbl_audio_nrf5_i2s_irq_handler, &s_speaker, 0);

const struct pbl_audio *const SPEAKER = &s_speaker.audio;

/* sensor SPI bus */

/* asterix shares SPI with flash, which we don't support */

PBL_PWM_NRF5_DEFINE(s_pwm0, "pwm0", 0, nullptr, PBL_GPIO(&dt_gpio0, 26, 0));

PBL_BACKLIGHT_PWM_DEFINE(s_backlight, .enable = PBL_GPIO(&dt_gpio1, 8, 0),
                         .pwm = PBL_PWM(&s_pwm0.ctrl, 0, PBL_PWM_HZ(256), 0),
                         .max_duty_cycle_percent = 67);

const struct pbl_backlight *const BACKLIGHT = &s_backlight.backlight;

const struct pbl_temp *const DIE_TEMP = NRF5_TEMP;

const struct pbl_rtc *const RTC = RETAINED_RTC;
