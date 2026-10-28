/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/display.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/pwm.h>
#include <pbl/drivers/spi.h>

/**
 * @defgroup drivers_display_sharp_ls013b7dh01 Sharp LS013B7DH01
 * @ingroup drivers_display
 * @brief @ref drivers_display driver for the Sharp LS013B7DH01 memory LCD, an SPI device.
 *
 * The 144x168 panel keeps its image in pixel memory and takes @ref PBL_DISPLAY_FORMAT_MONO rows
 * over SPI, least significant bit first, with an active-high chip select. Its VCOM is inverted on
 * the rising edges of EXTCOMIN, driven by a PWM output while the panel is on. Updates complete
 * from the SPI controller's interrupt.
 *
 * @code{.c}
 * PBL_DISPLAY_SHARP_LS013B7DH01_DEFINE(s_display, "display",
 *     PBL_SPI_DEV(&s_spi3.bus, 0, 1000000, PBL_SPI_MODE_0 | PBL_SPI_CS_HIGH | PBL_SPI_LSB_FIRST),
 *     PBL_GPIO(NRF5_GPIO_P0, 4, 0), PBL_PWM(&s_extcomin.ctrl, 0, PBL_PWM_HZ(120), 0), false);
 * @endcode
 * @{
 */

/** @brief Width, in pixels. */
#define SHARP_LS013B7DH01_WIDTH 144
/** @brief Height, in pixels. */
#define SHARP_LS013B7DH01_HEIGHT 168

/** @cond INTERNAL_HIDDEN */
#define SHARP_LS013B7DH01_LINE_BYTES (SHARP_LS013B7DH01_WIDTH / 8)

struct pbl_display_sharp_ls013b7dh01_state {
  uint8_t buf[2 + ((SHARP_LS013B7DH01_LINE_BYTES + 2) * SHARP_LS013B7DH01_HEIGHT)];
  struct pbl_spi_xfer xfer;
  bool rotated;
};
/** @endcond */

/** @brief Sharp LS013B7DH01 display. */
struct pbl_display_sharp_ls013b7dh01 {
  /** Display. */
  struct pbl_display disp;
  /** Driver runtime state. */
  struct pbl_display_sharp_ls013b7dh01_state *state;
  /** SPI device. */
  struct pbl_spi_dev spi;
  /** DISP input, turning the image on. */
  struct pbl_gpio enable;
  /** EXTCOMIN output. */
  struct pbl_pwm extcomin;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_display_ops pbl_display_sharp_ls013b7dh01_ops;
/** @endcond */

/**
 * @brief Define a Sharp LS013B7DH01 display.
 *
 * @param sym Symbol of the display.
 * @param _name Name.
 * @param _spi SPI device, from PBL_SPI_DEV().
 * @param _enable DISP input, from PBL_GPIO().
 * @param _extcomin EXTCOMIN output, from PBL_PWM(), at twice the VCOM frequency.
 * @param _rotated Panel mounted upside down.
 */
#define PBL_DISPLAY_SHARP_LS013B7DH01_DEFINE(sym, _name, _spi, _enable, _extcomin, _rotated) \
  PBL_DISPLAY_STATE_DEFINE(sym);                                                             \
  static struct pbl_display_sharp_ls013b7dh01_state sym##_sharp_state;                       \
  const struct pbl_display_sharp_ls013b7dh01 sym = {                                         \
    .disp = PBL_DISPLAY_INIT(sym, _name, &pbl_display_sharp_ls013b7dh01_ops, nullptr,        \
                             SHARP_LS013B7DH01_WIDTH, SHARP_LS013B7DH01_HEIGHT,              \
                             PBL_DISPLAY_FORMAT_MONO, _rotated),                             \
    .state = &sym##_sharp_state,                                                             \
    .spi = _spi,                                                                             \
    .enable = _enable,                                                                       \
    .extcomin = _extcomin,                                                                   \
  };                                                                                         \
  PBL_DEVICE_REGISTER(sym, &sym.disp.dev)

/** @} */
