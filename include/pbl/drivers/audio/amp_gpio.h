/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/regulator.h>

/**
 * @defgroup drivers_audio_amp_gpio GPIO amplifier
 * @ingroup drivers_audio
 * @brief Amplifier switched by an enable GPIO, with an optional supply.
 *
 * Some class-D amplifiers select their mode from the number of pulses on the enable line before
 * it stays active: the driver sends @c mode_pulses pulses of @c pulse_us high and low first.
 *
 * @code{.c}
 * PBL_AUDIO_AMP_GPIO_DEFINE(s_amp, "amp", PBL_GPIO(SF32LB_GPIO1, 0, 0), nullptr,
 *                           .mode_pulses = 1, .pulse_us = 200, .settle_us = 2000);
 * @endcode
 * @{
 */

/** @brief GPIO amplifier. */
struct pbl_audio_amp_gpio {
  /** Amplifier. */
  struct pbl_audio_amp amp;
  /** Enable line. */
  struct pbl_gpio enable;
  /** Supply, or NULL. */
  const struct pbl_regulator *supply;
  /** Mode pulses sent before enabling. */
  uint8_t mode_pulses;
  /** Length of each half of a mode pulse, in microseconds. */
  uint16_t pulse_us;
  /** Time to settle once the line is driven inactive at initialization, in microseconds. */
  uint16_t settle_us;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio_amp_ops pbl_audio_amp_gpio_ops;
int pbl_audio_amp_gpio_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define a GPIO amplifier.
 *
 * @param sym Symbol of the amplifier.
 * @param _name Device name.
 * @param _enable Enable line, from PBL_GPIO().
 * @param _supply Supply, or NULL.
 * @param ... Further fields: @c mode_pulses, @c pulse_us, @c settle_us.
 */
#define PBL_AUDIO_AMP_GPIO_DEFINE(sym, _name, _enable, _supply, ...)                     \
  PBL_DEVICE_STATE_DEFINE(sym);                                                          \
  static const struct pbl_audio_amp_gpio sym = {                                         \
    .amp =                                                                               \
        {                                                                                \
          .dev = PBL_DEVICE_INIT(sym, _name, pbl_audio_amp_gpio_init, nullptr, nullptr), \
          .ops = &pbl_audio_amp_gpio_ops,                                                \
        },                                                                               \
    .enable = _enable,                                                                   \
    .supply = (_supply),                                                                 \
    __VA_ARGS__                                                                          \
  };                                                                                     \
  PBL_DEVICE_REGISTER(sym, &sym.amp.dev)

/** @} */
