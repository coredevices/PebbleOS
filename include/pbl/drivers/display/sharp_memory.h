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
 * @defgroup drivers_display_sharp_memory Sharp memory LCDs
 * @ingroup drivers_display
 * @brief @ref drivers_display driver for the Sharp memory LCDs, SPI devices.
 *
 * The panels keep their image in pixel memory and take @ref PBL_DISPLAY_FORMAT_MONO rows over
 * SPI, with an active-high chip select. They expect every byte least significant bit first: with
 * @ref PBL_SPI_LSB_FIRST the controller shifts them so, otherwise the driver reverses their bits
 * for a controller shifting most significant bit first. VCOM is inverted on the rising edges of
 * EXTCOMIN, driven by a PWM output while the panel is on. Updates complete from the SPI
 * controller.
 *
 * An update is staged in a transfer buffer of @c CONFIG_DISPLAY_MAX_UPDATE_ROWS rows, or of the
 * whole panel if that is 0, and takes at most as many: a smaller buffer for a large panel, sent
 * in more updates.
 *
 * @code{.c}
 * PBL_DISPLAY_SHARP_MEMORY_DEFINE(s_display, "display", 144, 168, &s_spi3, 0, 1000000,
 *                                 PBL_SPI_MODE_0 | PBL_SPI_CS_HIGH | PBL_SPI_LSB_FIRST,
 *                                 PBL_GPIO(&dt_gpio0, 4, 0), 0,
 *                                 PBL_PWM(&s_extcomin, 0, PBL_PWM_HZ(120), 0));
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
// Mode byte, then per line its address, its data and a dummy byte, then a last dummy byte, for
// the most rows an update takes
#define SHARP_MEMORY_BUF_SIZE(_width, _height) \
  (2U + (((_width) / 8U) + 2U) * PBL_DISPLAY_MAX_UPDATE_ROWS(_height))

struct pbl_display_sharp_memory_state {
  struct pbl_spi_xfer xfer;
  bool rotated;
};
/** @endcond */

/** @brief Sharp memory LCD. */
struct pbl_display_sharp_memory {
  /** Display. */
  struct pbl_display disp;
  /** Driver runtime state. */
  struct pbl_display_sharp_memory_state *state;
  /** Transfer buffer, of SHARP_MEMORY_BUF_SIZE() bytes. */
  uint8_t *buf;
  /** SPI device. */
  struct pbl_spi_dev spi;
  /** DISP input, turning the image on. */
  struct pbl_gpio enable;
  /** EXTCOMIN output. */
  struct pbl_pwm extcomin;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_display_ops pbl_display_sharp_memory_ops;
/** @endcond */

/**
 * @brief Define a Sharp memory LCD.
 *
 * @param sym Symbol of the display.
 * @param _name Name.
 * @param _width Width, in pixels, a multiple of 8.
 * @param _height Height, in pixels, up to 255.
 * @param _bus SPI bus.
 * @param _cs Chip select number.
 * @param _max_hz Clock frequency, in Hz.
 * @param _mode SPI mode flags, @c PBL_SPI_*.
 * @param _enable DISP input, from PBL_GPIO().
 * @param _rotation Panel rotation, 0 or 180 degrees.
 * @param ... EXTCOMIN output, from PBL_PWM(), at twice the VCOM frequency.
 */
#define PBL_DISPLAY_SHARP_MEMORY_DEFINE(sym, _name, _width, _height, _bus, _cs, _max_hz, _mode, \
                                        _enable, _rotation, ...)                                \
  static_assert((_width) % 8U == 0U && (_height) <= 255U, "panel size");                        \
  static_assert((_rotation) == 0 || (_rotation) == 180, "rotation of 0 or 180 degrees");        \
  PBL_DISPLAY_STATE_DEFINE(sym);                                                                \
  static struct pbl_display_sharp_memory_state sym##_sharp_state;                               \
  static uint8_t sym##_sharp_buf[SHARP_MEMORY_BUF_SIZE(_width, _height)];                       \
  PBL_DEVICE_INSTANCE(sym, struct pbl_display_sharp_memory, struct pbl_display, disp) {         \
    .disp = PBL_DISPLAY_INIT(sym, _name, nullptr, &pbl_display_sharp_memory_ops, (_width),      \
                             (_height), PBL_DISPLAY_FORMAT_MONO, (_rotation) == 180,            \
                             .max_update_rows = PBL_DISPLAY_MAX_UPDATE_ROWS(_height)),          \
    .state = &sym##_sharp_state, .buf = sym##_sharp_buf,                                        \
    .spi = PBL_SPI_DEV(_bus, _cs, _max_hz, _mode), .enable = _enable, .extcomin = __VA_ARGS__,  \
  }

/** @} */
