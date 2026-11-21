/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/sensor.h>

/**
 * @defgroup drivers_sensor_sim Simulators
 * @ingroup drivers_sensor
 * @brief Simulated sensors, measuring what their simulator API sets.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_sensor pbl_temp_sim;
/** @endcond */

/** @brief The simulated temperature sensor, at 25 degrees Celsius until set. */
#define SIM_TEMP (&pbl_temp_sim)

/**
 * @brief Set the temperature the simulated temperature sensor reads.
 *
 * @param sensor Sensor.
 * @param millicelsius Temperature in millidegrees Celsius.
 */
void pbl_temp_sim_set(const struct pbl_sensor *sensor, int32_t millicelsius);

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_sensor pbl_mag_sim;
/** @endcond */

/**
 * @brief The simulated magnetometer, measuring no field until set.
 *
 * While sampling, the field is reported once per sampling interval.
 */
#define SIM_MAG (&pbl_mag_sim)

/**
 * @brief Set the magnetic field the simulated magnetometer measures.
 *
 * @param sensor Sensor.
 * @param x Field along the watch X axis, in nanotesla.
 * @param y Field along the watch Y axis, in nanotesla.
 * @param z Field along the watch Z axis, in nanotesla.
 */
void pbl_mag_sim_set(const struct pbl_sensor *sensor, int32_t x, int32_t y, int32_t z);

/** @brief Number of samples the replay queue of the simulated accelerometer holds. */
#define PBL_ACCEL_SIM_QUEUE_SIZE 256U

/** @brief Simulated accelerometer sample. */
struct pbl_accel_sim_sample {
  /** Acceleration along the x axis, in milli-g. */
  int16_t x;
  /** Acceleration along the y axis, in milli-g. */
  int16_t y;
  /** Acceleration along the z axis, in milli-g. */
  int16_t z;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_sensor pbl_accel_sim;
/** @endcond */

/**
 * @brief The simulated accelerometer, replaying the samples and events it is fed.
 *
 * Fed samples are replayed one per sampling interval, the last one being latched once the queue
 * runs out: a watch lying still, face up, until the first one. The simulated part has no FIFO.
 */
#define SIM_ACCEL (&pbl_accel_sim)

/**
 * @brief Queue samples for the simulated accelerometer to replay.
 *
 * Samples that do not fit in the queue are dropped.
 *
 * @param sensor Sensor.
 * @param samples Samples, in the watch frame.
 * @param count Number of samples.
 * @return Number of samples queued.
 */
size_t pbl_accel_sim_feed(const struct pbl_sensor *sensor,
                          const struct pbl_accel_sim_sample *samples, size_t count);

/**
 * @brief Drop the queued samples, keeping the last one replayed latched.
 *
 * @param sensor Sensor.
 */
void pbl_accel_sim_flush(const struct pbl_sensor *sensor);

/**
 * @brief Report an event, as if the simulated accelerometer detected it.
 *
 * Dropped while detection of that event type is disabled, as on real hardware.
 *
 * @param sensor Sensor.
 * @param event Event, in the watch frame.
 */
void pbl_accel_sim_report_event(const struct pbl_sensor *sensor,
                                const struct pbl_sensor_event *event);

/** @} */
