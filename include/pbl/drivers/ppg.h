/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_ppg Photoplethysmography front end
 * @ingroup drivers
 * @brief Optical front end (PPG) class.
 *
 * A PPG front end is a struct pbl_ppg device. It lights LEDs and samples the light that comes back
 * on one or more receivers (photodiodes with a transimpedance amplifier and an ADC). The hardware
 * runs a fixed sequence of time slots: in every sampling period each enabled slot fires its LED
 * drivers and samples its receivers, producing one sample per receiver. What each slot lights is
 * board data, described by struct pbl_ppg_info; a slot with no LEDs measures ambient light.
 *
 * Drivers deliver raw samples only. Signal processing, including gain control, belongs to the
 * consumer, which adjusts LED currents and receiver gains with pbl_ppg_set_led_current() and
 * pbl_ppg_set_gain(). Drivers handle their interrupts in their own bottom half and hand the
 * samples to the listener a consumer registered with pbl_ppg_set_listener().
 *
 * Every call into the driver, and every report out of it, runs with the device lock held, a
 * recursive mutex.
 *
 * @code{.c}
 * static void prv_samples(const struct pbl_ppg *ppg, const struct pbl_ppg_batch *batch,
 *                         void *user_data) {
 *   ...
 * }
 *
 * static const struct pbl_ppg_listener s_listener = {.samples = prv_samples};
 *
 * const struct pbl_ppg_config config = {.sample_rate_hz = 25, .rx_mask = {0x1, 0x1}, .batch = 25};
 *
 * pbl_ppg_set_listener(PPG, &s_listener);
 * pbl_ppg_configure(PPG, &config);
 * pbl_ppg_start(PPG);
 * @endcode
 * @{
 */

/** @brief LED color. */
enum pbl_ppg_led_color {
  /** Green, around 525 nm. */
  PBL_PPG_LED_GREEN,
  /** Red, around 660 nm. */
  PBL_PPG_LED_RED,
  /** Infrared, around 940 nm. */
  PBL_PPG_LED_IR,
};

/** @brief Maximum number of slots. */
#define PBL_PPG_MAX_SLOTS 8
/** @brief Maximum number of LED drivers. */
#define PBL_PPG_MAX_LED_DRIVERS 2

/** @brief What a slot measures. */
struct pbl_ppg_slot {
  /** LED drivers fired, as a bitmask; 0 for an ambient light slot. */
  uint8_t led_mask;
  /** Color of the LED each fired driver lights in this slot. */
  enum pbl_ppg_led_color leds[PBL_PPG_MAX_LED_DRIVERS];
  /** Receivers that can be sampled, as a bitmask. */
  uint8_t rx_mask;
};

/** @brief Front end description. */
struct pbl_ppg_info {
  /** Slots. */
  const struct pbl_ppg_slot *slots;
  /** Number of slots. */
  uint8_t num_slots;
  /** Number of receivers. */
  uint8_t num_rx;
  /** Highest LED current, in microamperes. */
  uint32_t max_led_current_ua;
  /** Receiver gains, in ohms, in increasing order. */
  const uint32_t *gains_ohm;
  /** Number of receiver gains. */
  uint8_t num_gains;
  /** Positive full scale of a sample value. */
  int32_t full_scale;
  /** Receiver output voltage at positive full scale, in microvolts. */
  uint32_t full_scale_uv;
  /** Highest sample rate, in hertz. */
  uint16_t max_sample_rate_hz;
  /** Depth of the hardware FIFO, in samples. */
  uint16_t fifo_depth;
};

/** @brief The hardware changed the LED current or receiver gain before this sample. */
#define PBL_PPG_SAMPLE_GAIN_CHANGED (1U << 0)
/** @brief The receiver saturated. */
#define PBL_PPG_SAMPLE_SATURATED (1U << 1)

/** @brief A sample. */
struct pbl_ppg_sample {
  /** Signed value, at most pbl_ppg_info::full_scale in magnitude. */
  int32_t value;
  /** Slot. */
  uint8_t slot;
  /** Receiver. */
  uint8_t rx;
  /** Flags, @c PBL_PPG_SAMPLE_*. */
  uint8_t flags;
};

/**
 * @brief Batch of samples, oldest first.
 *
 * Within a sampling period, samples come in slot order. Periods are 1 / @c sample_rate_hz apart,
 * the last one ending at @c timestamp_ms.
 */
struct pbl_ppg_batch {
  /** Samples. */
  const struct pbl_ppg_sample *samples;
  /** Number of samples. */
  uint16_t num_samples;
  /** Sample rate, in hertz. */
  uint16_t sample_rate_hz;
  /** Time of the last sampling period, in milliseconds of uptime. */
  uint64_t timestamp_ms;
};

/** @brief Settings of a slot receiver. */
struct pbl_ppg_channel {
  /** Current set on each LED driver, in microamperes; 0 if not fired. */
  uint32_t led_current_ua[PBL_PPG_MAX_LED_DRIVERS];
  /** Current of each LED driver after calibration, in microamperes; 0 if not fired. */
  uint32_t calibrated_led_current_ua[PBL_PPG_MAX_LED_DRIVERS];
  /** Nominal gain, in ohms, one of pbl_ppg_info::gains_ohm. */
  uint32_t gain_ohm;
  /** Gain after calibration, in ohms. */
  uint32_t calibrated_gain_ohm;
};

/** @brief Sampling configuration. */
struct pbl_ppg_config {
  /** Sample rate, in hertz. */
  uint16_t sample_rate_hz;
  /** Receivers to sample in each slot, as bitmasks; slots with none do not run. */
  uint8_t rx_mask[PBL_PPG_MAX_SLOTS];
  /** Sampling periods the driver may batch before reporting, at least 1. */
  uint16_t batch;
};

struct pbl_ppg;

/**
 * @brief Consumer of a front end's samples.
 *
 * Called from thread context with the device lock held. Data passed to the callbacks is only
 * valid for the duration of the call.
 */
struct pbl_ppg_listener {
  /** New samples. */
  void (*samples)(const struct pbl_ppg *ppg, const struct pbl_ppg_batch *batch, void *user_data);
  /** Data passed to the callbacks. */
  void *user_data;
};

/**
 * @brief Driver operations.
 *
 * Called with the device lock held. See the class functions for their semantics.
 */
struct pbl_ppg_ops {
  /** Bring the hardware up, leaving it idle. Returns -ENODEV if the part does not answer. */
  int (*init)(const struct pbl_ppg *ppg);
  /** Get the front end description. */
  const struct pbl_ppg_info *(*get_info)(const struct pbl_ppg *ppg);
  /** Apply a sampling configuration, while stopped. */
  int (*configure)(const struct pbl_ppg *ppg, const struct pbl_ppg_config *config);
  /** Start sampling. */
  int (*start)(const struct pbl_ppg *ppg);
  /** Stop sampling and put the hardware in its lowest power state. */
  int (*stop)(const struct pbl_ppg *ppg);
  /** Set the current of LED driver @p led in slot @p slot. */
  int (*set_led_current)(const struct pbl_ppg *ppg, uint8_t slot, uint8_t led, uint32_t ua);
  /** Set the gain of receiver @p rx in slot @p slot. */
  int (*set_gain)(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx, uint32_t ohm);
  /** Get the settings of receiver @p rx in slot @p slot. */
  int (*get_channel)(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx,
                     struct pbl_ppg_channel *channel);
};

/** @brief Class runtime state. */
struct pbl_ppg_state {
  /** Device lock. */
  struct pbl_mutex lock;
  /** Listener, or NULL. */
  const struct pbl_ppg_listener *listener;
  /** Sampling. */
  bool running;
};

/** @brief A PPG front end. */
struct pbl_ppg {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_ppg_ops *ops;
  /** Runtime state. */
  struct pbl_ppg_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_ppg_class;
int pbl_ppg_dev_init(const struct pbl_device *dev);
/** @endcond */

/** @brief The board's PPG front end. */
extern const struct pbl_ppg *const PPG;

/**
 * @brief Define the class state of front end @p sym, for the @c PBL_*_DEFINE() macro of a driver.
 *
 * @param sym Symbol of the instance.
 */
#define PBL_PPG_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);   \
  static struct pbl_ppg_state sym##_ppg_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Initializer for the struct pbl_ppg of front end @p sym.
 *
 * @param sym Symbol of the instance, with its state defined by PBL_PPG_STATE_DEFINE().
 * @param _name Name.
 * @param _parent Parent device, such as the I2C bus, or NULL.
 * @param _deps Devices initialized first, from PBL_DEVICE_DEPS(), or NULL.
 * @param _ops Driver operations.
 */
#define PBL_PPG_INIT(sym, _name, _parent, _deps, _ops)                                          \
  {                                                                                             \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_ppg_dev_init, _parent, _deps, &pbl_ppg_class), \
    .ops = (_ops),                                                                              \
    .state = &sym##_ppg_state,                                                                  \
  }

/**
 * @brief Take the device lock.
 *
 * Recursive. A consumer takes it around its own state to serialize it with the driver.
 *
 * @param ppg Front end.
 */
void pbl_ppg_lock(const struct pbl_ppg *ppg);

/**
 * @brief Release the device lock.
 *
 * @param ppg Front end.
 */
void pbl_ppg_unlock(const struct pbl_ppg *ppg);

/**
 * @brief Set the listener.
 *
 * @param ppg Front end.
 * @param listener Listener, or NULL to drop samples.
 * @return 0 on success, or a negative errno.
 */
int pbl_ppg_set_listener(const struct pbl_ppg *ppg, const struct pbl_ppg_listener *listener);

/**
 * @brief Get the front end description.
 *
 * @param ppg Front end.
 * @return Description, or NULL if the device is not ready.
 */
const struct pbl_ppg_info *pbl_ppg_get_info(const struct pbl_ppg *ppg);

/**
 * @brief Apply a sampling configuration.
 *
 * @param ppg Front end.
 * @param config Configuration.
 * @return 0 on success, -EBUSY while sampling, -EINVAL for an unsupported configuration, or a
 *         negative errno.
 */
int pbl_ppg_configure(const struct pbl_ppg *ppg, const struct pbl_ppg_config *config);

/**
 * @brief Start sampling with the last configuration applied.
 *
 * @param ppg Front end.
 * @return 0 on success, -EALREADY if sampling, or a negative errno.
 */
int pbl_ppg_start(const struct pbl_ppg *ppg);

/**
 * @brief Stop sampling.
 *
 * Samples not yet reported are dropped and the hardware goes to its lowest power state.
 *
 * @param ppg Front end.
 * @return 0 on success, or a negative errno.
 */
int pbl_ppg_stop(const struct pbl_ppg *ppg);

/**
 * @brief Check whether the front end is sampling.
 *
 * @param ppg Front end.
 * @return True if sampling.
 */
bool pbl_ppg_is_running(const struct pbl_ppg *ppg);

/**
 * @brief Set the current of an LED driver in a slot.
 *
 * Takes effect from the next sampling period. The driver rounds down to a current it can set;
 * pbl_ppg_get_channel() returns it, and the current the LED gets after calibration.
 *
 * @param ppg Front end.
 * @param slot Slot.
 * @param led LED driver, one of pbl_ppg_slot::led_mask.
 * @param ua Current, in microamperes, at most pbl_ppg_info::max_led_current_ua.
 * @return 0 on success, -EINVAL for an invalid slot, LED or current, or a negative errno.
 */
int pbl_ppg_set_led_current(const struct pbl_ppg *ppg, uint8_t slot, uint8_t led, uint32_t ua);

/**
 * @brief Set the gain of a receiver in a slot.
 *
 * Takes effect from the next sampling period.
 *
 * @param ppg Front end.
 * @param slot Slot.
 * @param rx Receiver.
 * @param ohm Gain, one of pbl_ppg_info::gains_ohm.
 * @return 0 on success, -EINVAL for an invalid slot, receiver or gain, or a negative errno.
 */
int pbl_ppg_set_gain(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx, uint32_t ohm);

/**
 * @brief Get the settings of a receiver in a slot.
 *
 * @param ppg Front end.
 * @param slot Slot.
 * @param rx Receiver.
 * @param[out] channel Settings.
 * @return 0 on success, -EINVAL for an invalid slot or receiver, or a negative errno.
 */
int pbl_ppg_get_channel(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx,
                        struct pbl_ppg_channel *channel);

/**
 * @brief Convert a sample value to the receiver's photocurrent.
 *
 * @param info Front end description.
 * @param channel Settings the sample was taken with.
 * @param value Sample value.
 * @return Photocurrent, in picoamperes.
 */
int64_t pbl_ppg_value_to_pa(const struct pbl_ppg_info *info, const struct pbl_ppg_channel *channel,
                            int32_t value);

/**
 * @brief Report a batch of samples to the listener.
 *
 * Called by the driver from thread context with the device lock held.
 *
 * @param ppg Front end.
 * @param batch Batch, valid for the duration of the call.
 */
void pbl_ppg_report_samples(const struct pbl_ppg *ppg, const struct pbl_ppg_batch *batch);

/** @} */
