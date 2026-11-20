/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/work.h>
#include <pbl/util/bits.h>

/**
 * @defgroup drivers_sensor Sensor
 * @ingroup drivers
 * @brief Sensor class: accelerometers, gyroscopes, magnetometers, temperature and pressure
 * sensors.
 *
 * A sensor is a struct pbl_sensor device measuring one or more channel types, e.g. an IMU
 * measuring acceleration, angular rate and temperature. Drivers do not convert their data: they
 * hand over frames as the hardware produced them, together with channel descriptors saying where
 * each channel is in a frame, how it is encoded and how it scales to the canonical unit of its
 * type. The class decodes frames on behalf of consumers, applying the board's mount matrix and
 * the watch rotation to vector types, so a consumer sees every sensor in the watch frame and in
 * the same units.
 *
 * | Type                      | Values  | Unit                       |
 * | ------------------------- | ------- | -------------------------- |
 * | #PBL_SENSOR_ACCEL         | X, Y, Z | micro-g                    |
 * | #PBL_SENSOR_GYRO          | X, Y, Z | millidegrees per second    |
 * | #PBL_SENSOR_MAGN          | X, Y, Z | nanotesla                  |
 * | #PBL_SENSOR_TEMP          | 1       | millidegrees Celsius       |
 * | #PBL_SENSOR_PRESSURE      | 1       | millipascals               |
 *
 * pbl_sensor_read() returns the latest values of a type: a fresh conversion, or the latest frame
 * when the sensor is streaming. Streaming sensors report batches of frames to the listener a
 * consumer registered with pbl_sensor_set_listener(), at the sampling interval and batch size set
 * through attributes, and events such as shakes once enabled with pbl_sensor_set_event().
 *
 * Batches live in buffers owned by the class, sized by the driver and DMA-safe, so a driver can
 * read a hardware FIFO straight into one, from its interrupt if its bus allows. A driver reports
 * a batch from thread context with pbl_sensor_report(), or from any context with
 * pbl_sensor_submit(), which reports it from the high priority system work queue. Once the
 * listener returns the buffer goes back to the driver, unless the listener kept it with
 * pbl_sensor_hold() to work on it later.
 *
 * Every call into the driver, and every report out of it, runs with the device lock held, a
 * recursive mutex. The listener runs with it held too, so it may call back into the device.
 *
 * @code{.c}
 * int32_t millicelsius;
 *
 * if (pbl_sensor_read(DIE_TEMP, PBL_SENSOR_TEMP, &millicelsius, nullptr) == 0) {
 *   ...
 * }
 * @endcode
 *
 * Streaming:
 *
 * @code{.c}
 * static void prv_batch(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch,
 *                       void *user_data) {
 *   int32_t xyz[3];
 *
 *   for (uint16_t i = 0U; i < batch->num_frames; i++) {
 *     if (pbl_sensor_decode(sensor, batch, PBL_SENSOR_ACCEL, i, 1U, xyz) > 0) {
 *       ...
 *     }
 *   }
 * }
 *
 * static const struct pbl_sensor_listener s_listener = {.batch = prv_batch};
 *
 * int32_t interval_us = 40000;
 * int32_t frames = 25;
 *
 * pbl_sensor_set_listener(ACCEL, &s_listener);
 * pbl_sensor_attr_set(ACCEL, PBL_SENSOR_ACCEL, PBL_SENSOR_ATTR_INTERVAL, &interval_us);
 * pbl_sensor_attr_set(ACCEL, PBL_SENSOR_ACCEL, PBL_SENSOR_ATTR_BATCH, &frames);
 * @endcode
 * @{
 */

/** @brief Channel type. */
enum pbl_sensor_type {
  /** Acceleration, in micro-g. */
  PBL_SENSOR_ACCEL,
  /** Angular rate, in millidegrees per second. */
  PBL_SENSOR_GYRO,
  /** Magnetic field, in nanotesla. */
  PBL_SENSOR_MAGN,
  /** Temperature, in millidegrees Celsius. */
  PBL_SENSOR_TEMP,
  /** Pressure, in millipascals. */
  PBL_SENSOR_PRESSURE,
  /** Number of channel types. */
  PBL_SENSOR_TYPE_COUNT,
};

/** @brief Axis of a vector channel. */
enum pbl_sensor_axis {
  /** X axis; in the watch frame, toward the right of the watch. */
  PBL_SENSOR_AXIS_X,
  /** Y axis; in the watch frame, toward the top of the watch. */
  PBL_SENSOR_AXIS_Y,
  /** Z axis; in the watch frame, out of the watchface. */
  PBL_SENSOR_AXIS_Z,
};

/** @brief Encoding of a channel in a frame. */
enum pbl_sensor_format {
  /** Signed 8 bits. */
  PBL_SENSOR_FMT_S8,
  /** Signed 16 bits, little-endian. */
  PBL_SENSOR_FMT_S16_LE,
  /** Signed 16 bits, big-endian. */
  PBL_SENSOR_FMT_S16_BE,
  /** Unsigned 16 bits, little-endian. */
  PBL_SENSOR_FMT_U16_LE,
  /** Unsigned 16 bits, big-endian. */
  PBL_SENSOR_FMT_U16_BE,
  /** Signed 24 bits, little-endian. */
  PBL_SENSOR_FMT_S24_LE,
  /** Unsigned 24 bits, little-endian. */
  PBL_SENSOR_FMT_U24_LE,
  /** Signed 32 bits, little-endian. */
  PBL_SENSOR_FMT_S32_LE,
};

/**
 * @brief Channel descriptor: where a channel is in a frame and how to decode it.
 *
 * The value in the canonical unit of the type is <tt>raw * scale_num / scale_den + bias</tt>,
 * computed in 64 bits. A driver whose conversion is not linear converts the value itself and
 * stores it in the frame with a unit scale.
 */
struct pbl_sensor_chan {
  /** Type, an enum pbl_sensor_type. */
  uint8_t type;
  /** Axis in the sensor frame for vector types, an enum pbl_sensor_axis; 0 otherwise. */
  uint8_t axis;
  /** Byte offset in the frame. */
  uint8_t offset;
  /** Encoding, an enum pbl_sensor_format. */
  uint8_t format;
  /** Raw to canonical unit numerator. */
  int32_t scale_num;
  /** Raw to canonical unit denominator, not 0. */
  int32_t scale_den;
  /** Offset added after scaling, in the canonical unit. */
  int32_t bias;
};

/**
 * @brief Initializer for a struct pbl_sensor_chan.
 *
 * @param _type Type.
 * @param _axis Axis in the sensor frame, 0 for scalar types.
 * @param _offset Byte offset in the frame.
 * @param _format Encoding.
 * @param _num Scale numerator.
 * @param _den Scale denominator.
 * @param _bias Offset added after scaling.
 */
#define PBL_SENSOR_CHAN(_type, _axis, _offset, _format, _num, _den, _bias) \
  {                                                                        \
    .type = (_type),                                                       \
    .axis = (_axis),                                                       \
    .offset = (_offset),                                                   \
    .format = (_format),                                                   \
    .scale_num = (_num),                                                   \
    .scale_den = (_den),                                                   \
    .bias = (_bias),                                                       \
  }

/**
 * @brief Batch of frames, oldest first.
 *
 * Every frame holds the same channels at the same offsets. A driver whose hardware interleaves
 * channels sampled at different rates, e.g. tagged FIFO entries, reports one batch per channel
 * set.
 */
struct pbl_sensor_batch {
  /** First frame, in a buffer from pbl_sensor_buf_alloc(). */
  const uint8_t *data;
  /** Channels of each frame. */
  const struct pbl_sensor_chan *chans;
  /** Number of channels. */
  uint8_t num_chans;
  /** Bytes from one frame to the next. */
  uint8_t stride;
  /** Number of frames. */
  uint16_t num_frames;
  /** Time of the last frame, in microseconds of uptime. */
  uint64_t timestamp_us;
  /** Interval between frames, in microseconds. */
  uint32_t interval_us;
};

/** @brief Largest frame a driver returns from a read. */
#define PBL_SENSOR_FRAME_MAX 24U

/** @brief A single frame, as returned by the driver read operation. */
struct pbl_sensor_frame {
  /** Frame data. */
  uint8_t data[PBL_SENSOR_FRAME_MAX];
  /** Channels of the frame. */
  const struct pbl_sensor_chan *chans;
  /** Number of channels. */
  uint8_t num_chans;
  /** Time of the frame, in microseconds of uptime. */
  uint64_t timestamp_us;
};

/**
 * @brief Attribute.
 *
 * Set and read per channel type: a driver applying an attribute to the whole device, as the
 * batch size of a FIFO shared by all its channels, ignores the type.
 */
enum pbl_sensor_attr {
  /**
   * Sampling interval in microseconds. The driver selects the longest supported interval equal
   * to or shorter than the requested one, saturating at the shortest it supports, and may flush
   * queued frames first so that timestamps stay accurate. 0 stops sampling, except what enabled
   * events need.
   */
  PBL_SENSOR_ATTR_INTERVAL,
  /**
   * Maximum number of frames per batch: 0 reports no batches, 1 reports every frame as soon as
   * it is acquired, and n > 1 lets the driver queue up to n frames, an upper bound it can use to
   * save power. Lowering it below the frames already queued reports them first, possibly from
   * within the call. Drivers without a FIFO report every frame while sampling.
   */
  PBL_SENSOR_ATTR_BATCH,
  /** Read-only. Largest batch the driver supports, the depth of its FIFO. */
  PBL_SENSOR_ATTR_BATCH_MAX,
  /** Full-scale range, in the canonical unit of the type. */
  PBL_SENSOR_ATTR_FULL_SCALE,
  /**
   * Shake detection sensitivity, from 0 (highest threshold, least sensitive) to 100 (lowest
   * threshold, most sensitive). Does not enable shake detection.
   */
  PBL_SENSOR_ATTR_SHAKE_SENSITIVITY,
};

/** @brief Event type. */
enum pbl_sensor_event_type {
  /** Shake, a motion above the shake threshold. */
  PBL_SENSOR_EVENT_SHAKE,
  /** Double tap on the watch body. */
  PBL_SENSOR_EVENT_DOUBLE_TAP,
  /** Number of event types. */
  PBL_SENSOR_EVENT_COUNT,
};

/** @brief Event. */
struct pbl_sensor_event {
  /** Type. */
  enum pbl_sensor_event_type type;
  /** Axis it was detected on: in the sensor frame from the driver, the watch frame to listeners. */
  enum pbl_sensor_axis axis;
  /** Direction along the axis, 1 or -1. */
  int8_t direction;
};

/**
 * @brief Mounting of a sensor in the watch, from a devicetree mount-matrix.
 *
 * Watch axis @c i is sensor axis @c map[i] times @c dir[i].
 */
struct pbl_sensor_mount {
  /** Sensor axis feeding each watch axis. */
  uint8_t map[3];
  /** Direction of each watch axis, 1 or -1. */
  int8_t dir[3];
};

/** @cond INTERNAL_HIDDEN */
#define PBL_SENSOR_MOUNT_COL_(x, y, z) ((x) != 0 ? 0 : (y) != 0 ? 1 : 2)
/** @endcond */

/**
 * @brief Initializer for a struct pbl_sensor_mount, from the rows of a devicetree mount-matrix.
 */
#define PBL_SENSOR_MOUNT_MATRIX(xx, xy, xz, yx, yy, yz, zx, zy, zz)            \
  {                                                                            \
    .map =                                                                     \
        {PBL_SENSOR_MOUNT_COL_(xx, xy, xz), PBL_SENSOR_MOUNT_COL_(yx, yy, yz), \
         PBL_SENSOR_MOUNT_COL_(zx, zy, zz)},                                   \
    .dir = {(xx) + (xy) + (xz), (yx) + (yy) + (yz), (zx) + (zy) + (zz)},       \
  }

/** @brief Identity mounting, for sensors aligned with the watch or without vector types. */
#define PBL_SENSOR_MOUNT_IDENTITY PBL_SENSOR_MOUNT_MATRIX(1, 0, 0, 0, 1, 0, 0, 0, 1)

struct pbl_sensor;

/**
 * @brief Consumer of a sensor's batches and events.
 *
 * Called from thread context with the device lock held. Batches and events are only valid for
 * the duration of the call, unless held with pbl_sensor_hold().
 */
struct pbl_sensor_listener {
  /** Optional. New batch. */
  void (*batch)(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch,
                void *user_data);
  /** Optional. Detected event, in the watch frame. */
  void (*event)(const struct pbl_sensor *sensor, const struct pbl_sensor_event *event,
                void *user_data);
  /** Data passed to the callbacks. */
  void *user_data;
};

/**
 * @brief Driver operations.
 *
 * Called with the device lock held, only for the types and events the device declares. See the
 * class functions for their semantics.
 */
struct pbl_sensor_ops {
  /** Optional. Bring the hardware up. Returns 0, -ENODEV if the part does not answer, or a
   * negative errno. */
  int (*init)(const struct pbl_sensor *sensor);
  /** Fill @p frame with the latest values of @p type, and maybe other types it measures. */
  int (*read)(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
              struct pbl_sensor_frame *frame);
  /** Optional. Set an attribute, writing back the value applied. */
  int (*attr_set)(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                  enum pbl_sensor_attr attr, int32_t *val);
  /** Optional. Get an attribute. */
  int (*attr_get)(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                  enum pbl_sensor_attr attr, int32_t *val);
  /** Optional. Enable or disable the detection of an event. */
  int (*set_event)(const struct pbl_sensor *sensor, enum pbl_sensor_event_type type, bool on);
};

/** @cond INTERNAL_HIDDEN */
struct pbl_sensor_buf {
  struct pbl_sensor_batch batch;
  uint32_t seq;
  uint8_t owner;
};
/** @endcond */

/** @brief Class runtime state. */
struct pbl_sensor_state {
  /** Device lock. */
  struct pbl_mutex lock;
  /** @cond INTERNAL_HIDDEN */
  const struct pbl_sensor *sensor;
  const struct pbl_sensor_listener *listener;
  uint32_t events;
  bool rotated;
  struct pbl_sensor_buf *bufs;
  uint8_t *data;
  uint8_t num_bufs;
  uint16_t buf_size;
  uint32_t seq;
  struct pbl_work work;
  /** @endcond */
};

/** @brief A sensor. */
struct pbl_sensor {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_sensor_ops *ops;
  /** Runtime state. */
  struct pbl_sensor_state *state;
  /** Types measured, a bit per enum pbl_sensor_type. */
  uint16_t types;
  /** Events detected, a bit per enum pbl_sensor_event_type. */
  uint8_t events;
  /** Mounting in the watch. */
  struct pbl_sensor_mount mount;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_sensor_class;
int pbl_sensor_dev_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the class state of sensor @p sym, without batch buffers, for sensors that are
 * only read.
 *
 * @param sym Symbol of the sensor instance.
 */
#define PBL_SENSOR_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);      \
  static struct pbl_sensor_state sym##_sensor_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Define the class state of streaming sensor @p sym, with its batch buffers.
 *
 * @param sym Symbol of the sensor instance.
 * @param _num_bufs Number of buffers, at least 1, up to 32.
 * @param _buf_size Size of each buffer in bytes.
 */
#define PBL_SENSOR_STREAM_STATE_DEFINE(sym, _num_bufs, _buf_size)                             \
  PBL_DEVICE_STATE_DEFINE(sym);                                                               \
  static uint8_t sym##_sensor_data[(_num_bufs) * PBL_DCACHE_ROUND_UP(_buf_size)] PBL_ALIGNED( \
      PBL_DCACHE_LINE_SIZE);                                                                  \
  static struct pbl_sensor_buf sym##_sensor_bufs[(_num_bufs)];                                \
  static struct pbl_sensor_state sym##_sensor_state = {                                       \
    .lock = PBL_MUTEX_INITIALIZER,                                                            \
    .bufs = sym##_sensor_bufs,                                                                \
    .data = sym##_sensor_data,                                                                \
    .num_bufs = (_num_bufs),                                                                  \
    .buf_size = PBL_DCACHE_ROUND_UP(_buf_size),                                               \
  }

/**
 * @brief Initializer for the struct pbl_sensor of sensor @p sym.
 *
 * @param sym Symbol of the sensor instance, with its state defined by PBL_SENSOR_STATE_DEFINE()
 *            or PBL_SENSOR_STREAM_STATE_DEFINE().
 * @param _name Name.
 * @param _parent Parent device, such as the I2C bus, or NULL.
 * @param _ops Driver operations.
 * @param _types Types measured, PBL_BIT() of each enum pbl_sensor_type.
 * @param _events Events detected, PBL_BIT() of each enum pbl_sensor_event_type.
 * @param ... Mounting, a struct pbl_sensor_mount initializer such as PBL_SENSOR_MOUNT_MATRIX().
 */
#define PBL_SENSOR_INIT(sym, _name, _parent, _ops, _types, _events, ...)                       \
  {                                                                                            \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_sensor_dev_init, _parent, &pbl_sensor_class), \
    .ops = (_ops),                                                                             \
    .state = &sym##_sensor_state,                                                              \
    .types = (_types),                                                                         \
    .events = (_events),                                                                       \
    .mount = __VA_ARGS__,                                                                      \
  }

/**
 * @brief Get the number of values of a type: 3 for vectors, 1 otherwise.
 *
 * @param type Type.
 * @return Number of values.
 */
static inline uint8_t pbl_sensor_num_values(enum pbl_sensor_type type) {
  return (type == PBL_SENSOR_ACCEL || type == PBL_SENSOR_GYRO || type == PBL_SENSOR_MAGN) ? 3U : 1U;
}

/**
 * @brief Check whether a sensor measures a type.
 *
 * @param sensor Sensor.
 * @param type Type.
 * @return True if it does.
 */
static inline bool pbl_sensor_has_type(const struct pbl_sensor *sensor, enum pbl_sensor_type type) {
  return (sensor->types & PBL_BIT(type)) != 0U;
}

/**
 * @brief Check whether a sensor detects an event.
 *
 * @param sensor Sensor.
 * @param type Event type.
 * @return True if it does.
 */
static inline bool pbl_sensor_has_event(const struct pbl_sensor *sensor,
                                        enum pbl_sensor_event_type type) {
  return (sensor->events & PBL_BIT(type)) != 0U;
}

/**
 * @brief Get the time of a frame of a batch.
 *
 * @param batch Batch.
 * @param i Frame index.
 * @return Time of the frame, in microseconds of uptime.
 */
static inline uint64_t pbl_sensor_frame_time(const struct pbl_sensor_batch *batch, uint16_t i) {
  return batch->timestamp_us - (uint64_t)(batch->num_frames - 1U - i) * batch->interval_us;
}

/**
 * @brief Take the device lock.
 *
 * Recursive. A consumer takes it around its own state to serialize it with the driver.
 *
 * @param sensor Sensor.
 */
void pbl_sensor_lock(const struct pbl_sensor *sensor);

/**
 * @brief Release the device lock.
 *
 * @param sensor Sensor.
 */
void pbl_sensor_unlock(const struct pbl_sensor *sensor);

/**
 * @brief Read the latest values of a type.
 *
 * Runs a conversion if the sensor is not sampling, which may sleep the caller. The driver may
 * report queued batches from within this call.
 *
 * @param sensor Sensor.
 * @param type Type.
 * @param[out] val Values in the canonical unit, pbl_sensor_num_values() of them, in the watch
 *             frame for vectors.
 * @param[out] timestamp_us Time of the values in microseconds of uptime, or NULL.
 * @return 0, -ENODEV if the sensor is not ready, -ENOTSUP if it does not measure @p type,
 *         -ENODATA if it has no values yet, or a negative errno.
 */
int pbl_sensor_read(const struct pbl_sensor *sensor, enum pbl_sensor_type type, int32_t *val,
                    uint64_t *timestamp_us);

/**
 * @brief Set an attribute.
 *
 * @param sensor Sensor.
 * @param type Type the attribute applies to.
 * @param attr Attribute.
 * @param[in,out] val Requested value, updated to the value applied.
 * @return 0, -ENODEV if the sensor is not ready, -ENOTSUP if the attribute is not supported for
 *         @p type, -EACCES for a read-only attribute, or a negative errno.
 */
int pbl_sensor_attr_set(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val);

/**
 * @brief Get an attribute.
 *
 * @param sensor Sensor.
 * @param type Type the attribute applies to.
 * @param attr Attribute.
 * @param[out] val Value.
 * @return 0, -ENODEV if the sensor is not ready, -ENOTSUP if the attribute is not supported for
 *         @p type, or a negative errno.
 */
int pbl_sensor_attr_get(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val);

/**
 * @brief Set the listener of batches and events.
 *
 * @param sensor Sensor.
 * @param listener Listener, or NULL to remove it.
 */
void pbl_sensor_set_listener(const struct pbl_sensor *sensor,
                             const struct pbl_sensor_listener *listener);

/**
 * @brief Enable or disable the detection of an event.
 *
 * @param sensor Sensor.
 * @param type Event type.
 * @param on True to enable, false to disable.
 * @return 0, -ENODEV if the sensor is not ready, -ENOTSUP if it does not detect @p type, or a
 *         negative errno.
 */
int pbl_sensor_set_event(const struct pbl_sensor *sensor, enum pbl_sensor_event_type type, bool on);

/**
 * @brief Check whether the detection of an event is enabled.
 *
 * @param sensor Sensor.
 * @param type Event type.
 * @return True if enabled.
 */
bool pbl_sensor_get_event(const struct pbl_sensor *sensor, enum pbl_sensor_event_type type);

/**
 * @brief Set whether the watch is worn rotated by 180 degrees.
 *
 * When rotated, the X and Y values of vector types and the directions of events along them are
 * inverted.
 *
 * @param sensor Sensor.
 * @param rotated True if rotated.
 */
void pbl_sensor_set_rotated(const struct pbl_sensor *sensor, bool rotated);

/**
 * @brief Decode a type from frames of a batch.
 *
 * @param sensor Sensor that reported the batch.
 * @param batch Batch.
 * @param type Type.
 * @param first Index of the first frame.
 * @param count Number of frames.
 * @param[out] out Values in the canonical unit, pbl_sensor_num_values() per frame, in the watch
 *             frame for vectors.
 * @return Number of values per frame, or -ENOENT if the batch has no @p type.
 */
int pbl_sensor_decode(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch,
                      enum pbl_sensor_type type, uint16_t first, uint16_t count, int32_t *out);

/**
 * @brief Keep the buffer of a batch past the listener call.
 *
 * Called by the listener. Copy the batch itself, and give the buffer back with
 * pbl_sensor_release() once done with it. The driver reports nothing while it has no free buffer.
 *
 * @param sensor Sensor.
 * @param batch Batch.
 */
void pbl_sensor_hold(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch);

/**
 * @brief Give back the buffer of a batch kept with pbl_sensor_hold().
 *
 * Callable from any context.
 *
 * @param sensor Sensor.
 * @param batch Batch.
 */
void pbl_sensor_release(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch);

/**
 * @name Driver interface
 * @{
 */

/**
 * @brief Get a free batch buffer.
 *
 * For drivers. Callable from any context.
 *
 * @param sensor Sensor.
 * @param[out] size Size of the buffer in bytes, or NULL.
 * @return Buffer data, DMA-safe, or NULL if every buffer is in use.
 */
uint8_t *pbl_sensor_buf_alloc(const struct pbl_sensor *sensor, uint16_t *size);

/**
 * @brief Give back a buffer from pbl_sensor_buf_alloc() without reporting it.
 *
 * For drivers. Callable from any context.
 *
 * @param sensor Sensor.
 * @param data Buffer data, or a pointer into it.
 */
void pbl_sensor_buf_free(const struct pbl_sensor *sensor, const uint8_t *data);

/**
 * @brief Report a batch to the listener.
 *
 * For drivers, from thread context with the device lock held. The batch data is in a buffer from
 * pbl_sensor_buf_alloc(), which the class takes back.
 *
 * @param sensor Sensor.
 * @param batch Batch.
 */
void pbl_sensor_report(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch);

/**
 * @brief Report a batch to the listener from the high priority system work queue.
 *
 * For drivers. Callable from any context, ISRs included. Batches are reported in the order they
 * are submitted, as pbl_sensor_report() does.
 *
 * @param sensor Sensor.
 * @param batch Batch, copied.
 */
void pbl_sensor_submit(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch);

/**
 * @brief Report an event to the listener.
 *
 * For drivers, from thread context with the device lock held, while detection of that event type
 * is enabled. The axis is in the sensor frame.
 *
 * @param sensor Sensor.
 * @param event Event.
 */
void pbl_sensor_report_event(const struct pbl_sensor *sensor, const struct pbl_sensor_event *event);

/** @} */

/** @} */
