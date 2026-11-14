/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/adc.h>
#include <pbl/drivers/intc.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/kernel/work.h>
#include <pbl/util/size.h>

/**
 * @defgroup drivers_input_adc_keys ADC keys
 * @ingroup drivers_input
 * @brief Keys on a resistor ladder, read through an ADC channel.
 *
 * The key whose voltage is the nearest to the channel's is pressed, none if the key-up voltage is
 * the nearest. The channel is read once the line has settled after an interrupt that a press or
 * release raises.
 * @{
 */

/** @brief A key. */
struct pbl_adc_keys_key {
  /** Name, @c label. */
  const char *label;
  /** @ref PBL_INPUT_EV_KEY code, @c linux,code. */
  uint16_t code;
  /** Voltage while pressed, in microvolts, @c press-threshold-microvolt. */
  int32_t press_uv;
};

/** @brief Runtime state. */
struct pbl_adc_keys_state {
  /** Read of the channel. */
  struct pbl_delayed_work scan;
  /** Keys. */
  const struct pbl_adc_keys *keys;
  /** Key pressed, NULL if none. */
  const struct pbl_adc_keys_key *pressed;
};

/** @brief ADC keys. */
struct pbl_adc_keys {
  /** Device. */
  struct pbl_device dev;
  /** Channel, @c io-channels. */
  struct pbl_adc_channel channel;
  /** Voltage with no key pressed, in microvolts, @c keyup-threshold-microvolt. */
  int32_t keyup_uv;
  /** Interrupt on a press or release, @c interrupts. */
  struct pbl_irq_spec irq;
  /** Pad of the line, if the board sets it up, @c pinctrl-0. */
  struct pbl_pinctrl pinctrl;
  /** Keys. */
  const struct pbl_adc_keys_key *keys;
  /** Number of keys. */
  uint8_t num_keys;
  /** Runtime state. */
  struct pbl_adc_keys_state *state;
};

/** @cond INTERNAL_HIDDEN */
int pbl_adc_keys_init(const struct pbl_device *dev);

#define PBL_ADC_KEYS_KEY(_label, _code, _press_uv) \
  {                                                \
    .label = (_label),                             \
    .code = (_code),                               \
    .press_uv = (_press_uv),                       \
  }

#define PBL_ADC_KEYS_DEFINE(sym, _name, _channel, _keyup_uv, _irq, _pins, ...)    \
  static const struct pbl_adc_keys_key sym##_keys[] = {__VA_ARGS__};              \
  static struct pbl_adc_keys_state sym##_adc_keys_state;                          \
  PBL_DEVICE_STATE_DEFINE(sym);                                                   \
  extern const struct pbl_adc_keys sym##_inst;                                    \
  PBL_DEVICE_REGISTER(sym##_inst, &sym##_inst.dev);                               \
  PBL_DEVICE_ALIAS(sym, sym##_inst, struct pbl_adc_keys, struct pbl_device, dev); \
  const struct pbl_adc_keys sym##_inst = {                                        \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_adc_keys_init, nullptr, nullptr),      \
    .channel = _channel,                                                          \
    .keyup_uv = (_keyup_uv),                                                      \
    .irq = _irq,                                                                  \
    .pinctrl = PBL_PINCTRL(_pins, nullptr, 0U),                                   \
    .keys = sym##_keys,                                                           \
    .num_keys = ARRAY_LENGTH(sym##_keys),                                         \
    .state = &sym##_adc_keys_state,                                               \
  }
/** @endcond */

/** @} */
