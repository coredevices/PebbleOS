/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_adc ADC
 * @ingroup drivers
 * @brief Analog-to-digital converters, read one channel at a time.
 *
 * A consumer takes a channel from the @c io-channels property of its node, as a
 * struct pbl_adc_channel, and reads it in microvolts:
 *
 * @code{.c}
 * int32_t uv;
 * int ret = pbl_adc_read(&keys->channel, &uv);
 * @endcode
 *
 * Reads take the device lock and may sleep: they are for threads, not interrupts.
 * @{
 */

struct pbl_adc;

/** @brief Driver operations. */
struct pbl_adc_ops {
  /** Optional. Bring the hardware up. */
  int (*init)(const struct pbl_adc *adc);
  /** Convert a channel once, in microvolts. */
  int (*read)(const struct pbl_adc *adc, uint32_t channel, int32_t *microvolt);
};

/** @brief Runtime state. */
struct pbl_adc_state {
  /** Device lock. */
  struct pbl_mutex lock;
};

/** @brief ADC. */
struct pbl_adc {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_adc_ops *ops;
  /** Runtime state. */
  struct pbl_adc_state *state;
  /** Number of channels. */
  uint8_t num_channels;
};

/** @brief A channel of an ADC. */
struct pbl_adc_channel {
  /** ADC. */
  const struct pbl_adc *adc;
  /** Channel number. */
  uint32_t id;
};

/** @brief Initializer for a struct pbl_adc_channel. */
#define PBL_ADC_CHANNEL(_adc, _id) {.adc = (_adc), .id = (_id)}

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_adc_class;

int pbl_adc_dev_init(const struct pbl_device *dev);

#define PBL_ADC_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);   \
  static struct pbl_adc_state sym##_adc_state = {.lock = PBL_MUTEX_INITIALIZER}

#define PBL_ADC_INIT(sym, _name, _parent, _deps, _ops, _num_channels)                           \
  {                                                                                             \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_adc_dev_init, _parent, _deps, &pbl_adc_class), \
    .ops = (_ops),                                                                              \
    .state = &sym##_adc_state,                                                                  \
    .num_channels = (_num_channels),                                                            \
  }
/** @endcond */

/**
 * @brief Convert a channel once.
 *
 * @param channel Channel.
 * @param[out] microvolt Voltage at the input, in microvolts.
 * @retval 0 Success.
 * @retval -ENODEV The ADC is not ready.
 * @retval -EINVAL No such channel.
 * @return Another negative errno on a conversion error.
 */
int pbl_adc_read(const struct pbl_adc_channel *channel, int32_t *microvolt);

/** @} */
