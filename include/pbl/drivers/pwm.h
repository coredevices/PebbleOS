/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/util/bits.h>
#include <pbl/util/units.h>

/**
 * @defgroup drivers_pwm PWM
 * @ingroup drivers
 * @brief PWM controller class.
 *
 * A PWM controller is a struct pbl_pwm_controller device with numbered channels. A consumer holds
 * a struct pbl_pwm, what a devicetree @c pwms entry describes: controller, channel, period and
 * flags. As in Linux, the output is set by applying a whole state at once:
 *
 * @code{.c}
 * static const struct pbl_pwm s_pwm = PBL_PWM(&s_pwm0.ctrl, 0, PBL_PWM_HZ(256), 0);
 *
 * struct pbl_pwm_state state;
 *
 * pbl_pwm_init_state(&s_pwm, &state);
 * pbl_pwm_set_relative_duty_cycle(&state, 50, 100);
 * state.enabled = true;
 *
 * ret = pbl_pwm_apply(&s_pwm, &state);
 * @endcode
 *
 * Times are in nanoseconds. Controllers round the period and the duty cycle down to what they
 * can generate. A disabled output stays at its inactive level, low for the normal polarity. A
 * channel belongs to one consumer; calls are made from task context.
 * @{
 */

struct pbl_pwm_controller;

/** @brief Class of the PWM controllers. */
extern const struct pbl_device_class pbl_pwm_class;

/** @brief @p us microseconds, in nanoseconds. */
#define PBL_PWM_USEC(us) ((us) * (uint32_t)PBL_NSEC_PER_USEC)

/** @brief @p ms milliseconds, in nanoseconds. */
#define PBL_PWM_MSEC(ms) ((ms) * (uint32_t)PBL_NSEC_PER_MSEC)

/** @brief Period of a @p hz Hz output, in nanoseconds. */
#define PBL_PWM_HZ(hz) ((uint32_t)PBL_NSEC_PER_SEC / (hz))

/** @brief Spec flag: the output is inverted. */
#define PBL_PWM_POLARITY_INVERTED PBL_BIT(0)

/** @brief Output polarity. */
enum pbl_pwm_polarity {
  /** High for the duty cycle, then low. */
  PBL_PWM_POLARITY_NORMAL,
  /** Low for the duty cycle, then high. */
  PBL_PWM_POLARITY_INVERSED,
};

/** @brief Output state. */
struct pbl_pwm_state {
  /** Period, in nanoseconds. */
  uint32_t period;
  /** Active time per period, in nanoseconds, up to @ref period. */
  uint32_t duty_cycle;
  /** Polarity. */
  enum pbl_pwm_polarity polarity;
  /** Whether the output runs; when false the output is inactive. */
  bool enabled;
};

/** @brief A channel, as used by a consumer. */
struct pbl_pwm {
  /** Controller. */
  const struct pbl_pwm_controller *ctrl;
  /** Channel number, from 0. */
  uint8_t channel;
  /** Default period, in nanoseconds. */
  uint32_t period;
  /** Flags, @ref PBL_PWM_POLARITY_INVERTED. */
  uint32_t flags;
};

/**
 * @brief Initializer for a struct pbl_pwm.
 *
 * @param _ctrl Controller.
 * @param _channel Channel number, from 0.
 * @param _period Default period in nanoseconds, e.g. from PBL_PWM_HZ().
 * @param _flags Flags, @ref PBL_PWM_POLARITY_INVERTED or 0.
 */
#define PBL_PWM(_ctrl, _channel, _period, _flags) \
  {.ctrl = (_ctrl), .channel = (_channel), .period = (_period), .flags = (_flags)}

/** @brief PWM controller driver operations. */
struct pbl_pwm_controller_ops {
  /**
   * Apply a validated state to a channel, rounding times down. Returns 0, @c -EINVAL for a
   * state the controller cannot generate, or another negative errno.
   */
  int (*apply)(const struct pbl_pwm_controller *ctrl, uint8_t channel,
               const struct pbl_pwm_state *state);
};

/** @brief A PWM controller. */
struct pbl_pwm_controller {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_pwm_controller_ops *ops;
  /** Last applied state of each channel. */
  struct pbl_pwm_state *states;
  /** Number of channels. */
  uint8_t num_channels;
};

/**
 * @brief Define the class state of PWM controller @p sym, for the driver's define macro.
 *
 * @param sym Symbol of the controller instance.
 * @param _num_channels Number of channels.
 */
#define PBL_PWM_CONTROLLER_STATE_DEFINE(sym, _num_channels) \
  PBL_DEVICE_STATE_DEFINE(sym);                             \
  static struct pbl_pwm_state sym##_pwm_states[_num_channels]

/**
 * @brief Initializer for the struct pbl_pwm_controller of controller @p sym.
 *
 * @param sym Symbol of the controller instance, with its state defined by
 * PBL_PWM_CONTROLLER_STATE_DEFINE().
 * @param _name Name.
 * @param _init Device init function, or NULL.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 * @param _ops Driver operations.
 */
#define PBL_PWM_CONTROLLER_INIT(sym, _name, _init, _deps, _ops)                      \
  {                                                                                  \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, nullptr, _deps, &pbl_pwm_class), \
    .ops = (_ops),                                                                   \
    .states = sym##_pwm_states,                                                      \
    .num_channels = sizeof(sym##_pwm_states) / sizeof(sym##_pwm_states[0]),          \
  }

/**
 * @brief Read the last applied state of a channel; disabled before the first one.
 *
 * @param pwm Channel.
 * @param[out] state State.
 */
void pbl_pwm_get_state(const struct pbl_pwm *pwm, struct pbl_pwm_state *state);

/**
 * @brief Prepare a state from the channel's spec.
 *
 * Takes the last applied state, with the period and polarity of the spec and no duty cycle.
 *
 * @param pwm Channel.
 * @param[out] state State.
 */
void pbl_pwm_init_state(const struct pbl_pwm *pwm, struct pbl_pwm_state *state);

/**
 * @brief Set the duty cycle of a state to @p duty / @p scale of its period.
 *
 * @param state State, with its period set.
 * @param duty Duty cycle, up to @p scale.
 * @param scale Scale.
 * @return 0, or @c -EINVAL if @p scale is 0 or @p duty is above it.
 */
int pbl_pwm_set_relative_duty_cycle(struct pbl_pwm_state *state, uint32_t duty, uint32_t scale);

/**
 * @brief Apply a state to a channel.
 *
 * @param pwm Channel.
 * @param state State.
 * @return 0, @c -EINVAL for a state the controller cannot generate, or another negative errno.
 */
int pbl_pwm_apply(const struct pbl_pwm *pwm, const struct pbl_pwm_state *state);

/**
 * @brief Disable a channel, keeping the rest of its state.
 *
 * @param pwm Channel.
 * @return 0 or a negative errno.
 */
int pbl_pwm_disable(const struct pbl_pwm *pwm);

/** @} */
