/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/backlight.h>
#include <pbl/drivers/gpio.h>

/**
 * @defgroup drivers_backlight_aw9364e AW9364E
 * @ingroup drivers_backlight
 * @brief Awinic AW9364E LED driver @ref drivers_backlight driver.
 *
 * Brightness is set by pulse counting on a single enable line, in 16 steps.
 *
 * @code{.c}
 * PBL_AW9364E_DEFINE(s_aw9364e, PBL_GPIO(SF32LB_GPIO1, 1, 0));
 *
 * const struct pbl_backlight *const BACKLIGHT = &s_aw9364e.backlight;
 * @endcode
 * @{
 */

/** @brief AW9364E driver state, owned by the driver. */
struct pbl_aw9364e_state {
  /** Pulse count latched by the chip, 0 if shut down. */
  uint8_t pulse_count;
};

/** @brief AW9364E LED driver. */
struct pbl_aw9364e {
  /** Backlight. */
  struct pbl_backlight backlight;
  /** Driver state. */
  struct pbl_aw9364e_state *state;
  /** Enable line, also used for the dimming pulses. */
  struct pbl_gpio enable;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_backlight_ops pbl_aw9364e_ops;
/** @endcond */

/**
 * @brief Define an AW9364E.
 *
 * @param sym Symbol of the instance; the backlight is @c sym.backlight.
 * @param _enable Enable line (struct pbl_gpio).
 */
#define PBL_AW9364E_DEFINE(sym, _enable)                                        \
  PBL_BACKLIGHT_STATE_DEFINE(sym);                                              \
  static struct pbl_aw9364e_state sym##_aw9364e_state;                          \
  const struct pbl_aw9364e sym = {                                              \
    .backlight = PBL_BACKLIGHT_INIT(sym, "aw9364e", nullptr, &pbl_aw9364e_ops), \
    .state = &sym##_aw9364e_state,                                              \
    .enable = _enable,                                                          \
  };                                                                            \
  PBL_DEVICE_REGISTER(sym, &sym.backlight.dev)

/** @} */
