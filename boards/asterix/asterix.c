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

#ifdef CONFIG_FLASH
const struct pbl_flash *const FLASH = &dt_flash0;
#endif
/* PERIPHERAL ID 43 */

const struct pbl_uart *const CONSOLE_UART = &dt_uart0;
/* PERIPHERAL ID 8 */

/* display */

const uint8_t BOARD_DISPLAY_CORNERS[] = {3, 1, 1};
const uint8_t BOARD_DISPLAY_NUM_CORNERS = ARRAY_LENGTH(BOARD_DISPLAY_CORNERS);

#ifdef CONFIG_DISPLAY
const struct pbl_display *const DISPLAY = &dt_display;
#endif

/* PERIPHERAL ID 10 */

/* nPM1300 */
/* PERIPHERAL ID 9 */

#ifdef CONFIG_PMIC_NPM1300
const struct pbl_npm1300_charger *const NPM1300_CHARGER = &dt_pmic_charger;
#endif

/* peripheral I2C bus */
#ifdef CONFIG_VIBE
const struct pbl_vibe *const VIBE = &dt_drv2604;
#endif

#ifdef CONFIG_AMBIENT_LIGHT
const struct pbl_als *const ALS = &dt_opt3001;
#endif

#ifdef CONFIG_ACCEL
const struct pbl_accel *const ACCEL = &dt_lsm6dso;
#endif

#ifdef CONFIG_MAG
const struct pbl_mag *const MAG = &dt_mmc5603nj;
#endif

/* PERIPHERAL ID 11 */

#ifdef CONFIG_MIC
const struct pbl_audio *const MIC = &dt_mic;
#endif

#ifdef CONFIG_SPEAKER
const struct pbl_audio *const SPEAKER = &dt_speaker;
#endif

/* sensor SPI bus */

/* asterix shares SPI with flash, which we don't support */

#ifdef CONFIG_BACKLIGHT
const struct pbl_backlight *const BACKLIGHT = &dt_backlight;
#endif

#ifdef CONFIG_TEMPERATURE
const struct pbl_temp *const DIE_TEMP = NRF5_TEMP;
#endif

#ifdef CONFIG_RTC
const struct pbl_rtc *const RTC = RETAINED_RTC;
#endif
