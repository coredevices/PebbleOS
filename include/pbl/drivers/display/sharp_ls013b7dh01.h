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
 * PBL_DISPLAY_SHARP_LS013B7DH01_DEFINE(s_display, "display", &s_spi3, 0, 1000000,
 *                                      PBL_SPI_MODE_0 | PBL_SPI_CS_HIGH | PBL_SPI_LSB_FIRST,
 *                                      PBL_GPIO(&dt_gpio0, 4, 0), 0,
 *                                      PBL_PWM(&s_extcomin, 0, PBL_PWM_HZ(120), 0));
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
 * @param _bus SPI bus.
 * @param _cs Chip select number.
 * @param _max_hz Clock frequency, in Hz.
 * @param _mode SPI mode flags, @c PBL_SPI_*.
 * @param _enable DISP input, from PBL_GPIO().
 * @param _rotation Panel rotation, 0 or 180 degrees.
 * @param ... EXTCOMIN output, from PBL_PWM(), at twice the VCOM frequency.
 */
#define PBL_DISPLAY_SHARP_LS013B7DH01_DEFINE(sym, _name, _bus, _cs, _max_hz, _mode, _enable,       \
                                             _rotation, ...)                                       \
  static_assert((_rotation) == 0 || (_rotation) == 180, "rotation of 0 or 180 degrees");           \
  PBL_DISPLAY_STATE_DEFINE(sym);                                                                   \
  static struct pbl_display_sharp_ls013b7dh01_state sym##_sharp_state;                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_display_sharp_ls013b7dh01, struct pbl_display, disp) {       \
    .disp = PBL_DISPLAY_INIT(sym, _name, &pbl_display_sharp_ls013b7dh01_ops, nullptr,              \
                             SHARP_LS013B7DH01_WIDTH, SHARP_LS013B7DH01_HEIGHT,                    \
                             PBL_DISPLAY_FORMAT_MONO, (_rotation) == 180),                         \
    .state = &sym##_sharp_state, .spi = PBL_SPI_DEV(_bus, _cs, _max_hz, _mode), .enable = _enable, \
    .extcomin = __VA_ARGS__,                                                                       \
  }

/** @} */
