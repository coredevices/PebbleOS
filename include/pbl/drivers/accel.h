/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>
#include <pbl/services/imu/units.h>

/**
 * @defgroup drivers_accel Accelerometer
 * @ingroup drivers
 * @brief Accelerometer class.
 *
 * An accelerometer is a struct pbl_accel device. Drivers are a thin layer over the hardware: they
 * keep no sample buffers and know nothing about their consumers. They report samples, shakes and
 * double taps to the class, which hands them to the listener a consumer registered with
 * pbl_accel_set_listener(), the accelerometer service in practice. Hardware state such as FIFO
 * modes is hidden: the consumer states its requirements (sampling interval, batch size) and the
 * driver picks the hardware configuration.
 *
 * Every call into the driver, and every report out of it, runs with the device lock held, a
 * recursive mutex. The listener runs with it held too, so it may call back into the device, and a
 * consumer that takes the lock around its own state is serialized with the driver without a
 * second lock to order against it.
 *
 * @code{.c}
 * static void prv_samples(const struct pbl_accel *accel, const AccelRawBatch *batch,
 *                         void *user_data) {
 *   ...
 * }
 *
 * static const struct pbl_accel_listener s_listener = {.samples = prv_samples};
 *
 * uint32_t interval_us;
 *
 * pbl_accel_set_listener(ACCEL, &s_listener);
 * pbl_accel_set_sampling_interval(ACCEL, 40000, &interval_us);
 * pbl_accel_set_num_samples(ACCEL, MIN(25, pbl_accel_get_max_num_samples(ACCEL)));
 * @endcode
 * @{
 */

/** @brief Accelerometer sample. */
typedef struct {
  /** Sample time in microseconds since the epoch, with no guaranteed precision. */
  uint64_t timestamp_us;
  /** Acceleration along the x axis, in milli-g. */
  int16_t x;
  /** Acceleration along the y axis, in milli-g. */
  int16_t y;
  /** Acceleration along the z axis, in milli-g. */
  int16_t z;
} AccelDriverSample;

/**
 * @brief Batch of raw samples in a driver-owned buffer.
 *
 * Handed to the listener so it can scale and copy the samples in one pass
 * instead of taking a callback per sample.
 */
typedef struct {
  /** First raw sample, past any per-sample header. */
  const uint8_t *data;
  /** Number of samples in the buffer. */
  uint16_t num_samples;
  /** Bytes between consecutive samples, which lets the service skip e.g. FIFO tags. */
  uint8_t stride;
  /** Per-axis source, indexed by IMUCoordinateAxis; encodes axis remapping and direction. */
  struct {
    /** Byte offset of the little-endian int16 value within a sample. */
    uint8_t offset;
    /** Sign applied to the value, +1 or -1. */
    int8_t sign;
  } axis[3];
  /** Raw-count to milli-g numerator: mg = raw * scale_num / scale_den. */
  int32_t scale_num;
  /** Raw-count to milli-g denominator. */
  int32_t scale_den;
  /** Timestamp of the first sample, in microseconds since the epoch. */
  uint64_t first_timestamp_us;
  /** Interval between samples, in microseconds. */
  uint32_t sampling_interval_us;
} AccelRawBatch;

/** @brief Accelerometer event type. */
enum pbl_accel_event_type {
  /** Shake, a motion above the shake threshold. */
  PBL_ACCEL_EVENT_SHAKE,
  /** Double tap on the watch body. */
  PBL_ACCEL_EVENT_DOUBLE_TAP,
};

/** @brief Accelerometer event. */
struct pbl_accel_event {
  /** Type. */
  enum pbl_accel_event_type type;
  /** Axis the event was detected on. */
  IMUCoordinateAxis axis;
  /** Direction along the axis: positive or negative. */
  int32_t direction;
};

struct pbl_accel;

/**
 * @brief Consumer of an accelerometer's samples and events.
 *
 * Called from thread context with the device lock held. Data passed to the callbacks is only
 * valid for the duration of the call.
 */
struct pbl_accel_listener {
  /** Optional. New samples, oldest first. */
  void (*samples)(const struct pbl_accel *accel, const AccelRawBatch *batch, void *user_data);
  /** Optional. Detected event. */
  void (*event)(const struct pbl_accel *accel, const struct pbl_accel_event *event,
                void *user_data);
  /** Data passed to the callbacks. */
  void *user_data;
};

/**
 * @brief Driver operations.
 *
 * Called with the device lock held. See the class functions for their semantics.
 */
struct pbl_accel_ops {
  /** Bring the hardware up. Returns 0, -ENODEV if the part does not answer, or a negative errno. */
  int (*init)(const struct pbl_accel *accel);
  /** Optional. Set whether the sensor is mounted rotated by 180 degrees. */
  int (*set_rotated)(const struct pbl_accel *accel, bool rotated);
  /** Set the sampling interval, returning the one used in @p actual_us. */
  int (*set_sampling_interval)(const struct pbl_accel *accel, uint32_t interval_us,
                               uint32_t *actual_us);
  /** Get the sampling interval. */
  uint32_t (*get_sampling_interval)(const struct pbl_accel *accel);
  /** Set the maximum number of samples to batch. */
  int (*set_num_samples)(const struct pbl_accel *accel, uint32_t num_samples);
  /** Get the depth of the hardware FIFO. */
  uint32_t (*get_max_num_samples)(const struct pbl_accel *accel);
  /** Read the most recent sample. */
  int (*peek)(const struct pbl_accel *accel, AccelDriverSample *sample);
  /** Optional. Enable or disable shake detection. */
  int (*set_shake_detection)(const struct pbl_accel *accel, bool on);
  /** Optional. Enable or disable double tap detection. */
  int (*set_double_tap_detection)(const struct pbl_accel *accel, bool on);
  /** Optional. Select high or normal shake sensitivity. */
  int (*set_shake_sensitivity_high)(const struct pbl_accel *accel, bool high);
  /** Optional. Set the normal shake sensitivity, 0 to 100. */
  int (*set_shake_sensitivity_percent)(const struct pbl_accel *accel, uint8_t percent);
};

/** @brief Class runtime state. */
struct pbl_accel_state {
  /** Device lock. */
  struct pbl_mutex lock;
  /** Listener, or NULL. */
  const struct pbl_accel_listener *listener;
  /** Shake detection enabled. */
  bool shake_detection;
  /** Double tap detection enabled. */
  bool double_tap_detection;
};

/** @brief An accelerometer. */
struct pbl_accel {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_accel_ops *ops;
  /** Runtime state. */
  struct pbl_accel_state *state;
};

/** @brief The board's accelerometer. */
extern const struct pbl_accel *const ACCEL;

/**
 * @brief Define the class state of accelerometer @p sym, for the @c PBL_*_DEFINE() macro of a
 * driver.
 *
 * @param sym Symbol of the accelerometer instance.
 */
#define PBL_ACCEL_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);     \
  static struct pbl_accel_state sym##_accel_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Initializer for the struct pbl_accel of accelerometer @p sym.
 *
 * @param sym Symbol of the accelerometer instance, with its state defined by
 *            PBL_ACCEL_STATE_DEFINE().
 * @param _name Name.
 * @param _parent Parent device, such as the I2C bus, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_ACCEL_INIT(sym, _name, _parent, _ops)                             \
  {                                                                           \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_accel_dev_init, _parent, nullptr), \
    .ops = (_ops),                                                            \
    .state = &sym##_accel_state,                                              \
  }

/**
 * @brief Device init of every accelerometer: calls the driver's init with the lock held.
 *
 * @param dev Accelerometer device.
 * @return 0 or a negative errno.
 */
int pbl_accel_dev_init(const struct pbl_device *dev);

/**
 * @brief Take the device lock.
 *
 * Recursive. A consumer takes it around its own state to serialize it with the driver.
 *
 * @param accel Accelerometer.
 */
void pbl_accel_lock(const struct pbl_accel *accel);

/**
 * @brief Release the device lock.
 *
 * @param accel Accelerometer.
 */
void pbl_accel_unlock(const struct pbl_accel *accel);

/**
 * @brief Set the listener of samples and events.
 *
 * @param accel Accelerometer.
 * @param listener Listener, or NULL to remove it.
 * @return 0 on success, or a negative errno.
 */
int pbl_accel_set_listener(const struct pbl_accel *accel,
                           const struct pbl_accel_listener *listener);

/**
 * @brief Set whether the axes are rotated by 180 degrees.
 *
 * @param accel Accelerometer.
 * @param rotated True if the sensor is mounted rotated by 180 degrees.
 * @return 0 on success, or a negative errno.
 */
int pbl_accel_set_rotated(const struct pbl_accel *accel, bool rotated);

/**
 * @brief Set the sampling interval.
 *
 * The driver selects the longest supported interval that is equal to or shorter than the
 * requested one, saturating at the shortest interval the hardware supports. The new interval
 * takes effect immediately; the driver may flush queued samples first so that timestamps stay
 * accurate.
 *
 * @param accel Accelerometer.
 * @param interval_us Requested sampling interval in microseconds.
 * @param[out] actual_us Sampling interval used, in microseconds, or NULL.
 * @return 0 on success, or a negative errno.
 */
int pbl_accel_set_sampling_interval(const struct pbl_accel *accel, uint32_t interval_us,
                                    uint32_t *actual_us);

/**
 * @brief Get the sampling interval.
 *
 * @param accel Accelerometer.
 * @return Sampling interval in microseconds, 0 if the device is not ready.
 */
uint32_t pbl_accel_get_sampling_interval(const struct pbl_accel *accel);

/**
 * @brief Set the maximum number of samples the driver may batch.
 *
 * - 0: the driver reports no samples.
 * - 1: the driver reports every sample as soon as it is acquired.
 * - n > 1: the driver may queue up to n samples and then report them at once. This is only an
 *   upper bound, which the driver can use for power saving.
 *
 * When n is lowered below the number of samples already queued, the driver reports them before
 * the new value takes effect, possibly from within this call.
 *
 * @param accel Accelerometer.
 * @param num_samples Maximum number of samples to batch, at most pbl_accel_get_max_num_samples().
 * @return 0 on success, or a negative errno.
 */
int pbl_accel_set_num_samples(const struct pbl_accel *accel, uint32_t num_samples);

/**
 * @brief Get the maximum number of samples the driver can batch.
 *
 * @param accel Accelerometer.
 * @return Depth of the hardware FIFO, 0 if the device is not ready.
 */
uint32_t pbl_accel_get_max_num_samples(const struct pbl_accel *accel);

/**
 * @brief Read the most recent sample.
 *
 * The driver may report queued samples from within this call.
 *
 * @param accel Accelerometer.
 * @param[out] sample Most recent sample.
 * @return 0 on success, or a negative errno.
 */
int pbl_accel_peek(const struct pbl_accel *accel, AccelDriverSample *sample);

/**
 * @brief Enable or disable shake detection.
 *
 * @param accel Accelerometer.
 * @param on True to enable, false to disable.
 * @return 0 on success, -ENOTSUP if the driver has no shake detection, or a negative errno.
 */
int pbl_accel_set_shake_detection(const struct pbl_accel *accel, bool on);

/**
 * @brief Check whether shake detection is enabled.
 *
 * @param accel Accelerometer.
 * @return True if shake detection is enabled.
 */
bool pbl_accel_get_shake_detection(const struct pbl_accel *accel);

/**
 * @brief Enable or disable double tap detection.
 *
 * @param accel Accelerometer.
 * @param on True to enable, false to disable.
 * @return 0 on success, -ENOTSUP if the driver has no double tap detection, or a negative errno.
 */
int pbl_accel_set_double_tap_detection(const struct pbl_accel *accel, bool on);

/**
 * @brief Check whether double tap detection is enabled.
 *
 * @param accel Accelerometer.
 * @return True if double tap detection is enabled.
 */
bool pbl_accel_get_double_tap_detection(const struct pbl_accel *accel);

/**
 * @brief Select high or normal shake sensitivity.
 *
 * In high sensitivity mode the detection threshold is at its minimum, so that any minor motion
 * is reported as a shake; otherwise the threshold set by pbl_accel_set_shake_sensitivity_percent()
 * applies. Does not enable shake detection.
 *
 * @param accel Accelerometer.
 * @param high True for high sensitivity, false for normal.
 * @return 0 on success, or a negative errno.
 */
int pbl_accel_set_shake_sensitivity_high(const struct pbl_accel *accel, bool high);

/**
 * @brief Set the normal shake sensitivity.
 *
 * Does not enable shake detection.
 *
 * @param accel Accelerometer.
 * @param percent Sensitivity from 0 (highest threshold, least sensitive) to 100 (lowest
 *                threshold, most sensitive).
 * @return 0 on success, or a negative errno.
 */
int pbl_accel_set_shake_sensitivity_percent(const struct pbl_accel *accel, uint8_t percent);

/**
 * @brief Report a batch of samples to the listener.
 *
 * Called by the driver from thread context with the device lock held, samples in increasing time
 * order.
 *
 * @param accel Accelerometer.
 * @param batch Batch, valid for the duration of the call.
 */
void pbl_accel_report_samples(const struct pbl_accel *accel, const AccelRawBatch *batch);

/**
 * @brief Report a single sample to the listener, as a one-sample batch.
 *
 * Same rules as pbl_accel_report_samples().
 *
 * @param accel Accelerometer.
 * @param sample Sample.
 */
void pbl_accel_report_sample(const struct pbl_accel *accel, const AccelDriverSample *sample);

/**
 * @brief Report an event to the listener.
 *
 * Called by the driver from thread context with the device lock held, while detection of that
 * event type is enabled.
 *
 * @param accel Accelerometer.
 * @param event Event.
 */
void pbl_accel_report_event(const struct pbl_accel *accel, const struct pbl_accel_event *event);

/** @} */
