/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>
#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/drivers/rtc/retained.h>
#include <pbl/drivers/temperature/nrf5.h>
#include <pbl/drivers/uart/nrf5.h>
#include <pbl/kernel/irq.h>
#include <pbl/util/size.h>

#include <devicetree/devices.h>
#include <hal/nrf_gpio.h>

const struct pbl_flash *const FLASH = &dt_flash0;
/* PERIPHERAL ID 43 */

const struct pbl_uart *const CONSOLE_UART = &dt_uart0;
/* PERIPHERAL ID 8 */

/* display */

const uint8_t BOARD_DISPLAY_CORNERS[] = {3, 1, 1};
const uint8_t BOARD_DISPLAY_NUM_CORNERS = ARRAY_LENGTH(BOARD_DISPLAY_CORNERS);

const struct pbl_display *const DISPLAY = &dt_display;

/* PERIPHERAL ID 10 */

/* nPM1300 */
/* PERIPHERAL ID 9 */

const struct pbl_npm1300_charger *const NPM1300_CHARGER = &dt_pmic_charger;

/* peripheral I2C bus */
const struct pbl_vibe *const VIBE = &dt_drv2604;

const struct pbl_als *const ALS = &dt_opt3001;

const struct pbl_accel *const ACCEL = &dt_lsm6dso;

const struct pbl_mag *const MAG = &dt_mmc5603nj;

/* PERIPHERAL ID 11 */

const struct pbl_audio *const MIC = &dt_mic;

const struct pbl_audio *const SPEAKER = &dt_speaker;

/* sensor SPI bus */

/* asterix shares SPI with flash, which we don't support */

const struct pbl_backlight *const BACKLIGHT = &dt_backlight;

const struct pbl_temp *const DIE_TEMP = NRF5_TEMP;

const struct pbl_rtc *const RTC = RETAINED_RTC;
