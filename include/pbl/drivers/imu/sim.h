/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/accel.h>

/**
 * @defgroup drivers_accel_sim Simulator
 * @ingroup drivers_accel
 * @brief Simulated accelerometer, replaying the samples and events it is fed.
 *
 * Fed samples are replayed one per sampling interval, the last one being latched once the queue
 * runs out: a watch lying still, face up, until the first one. The simulated part has no FIFO.
 * @{
 */

/** @brief Number of samples the replay queue holds. */
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
extern const struct pbl_accel pbl_accel_sim;
/** @endcond */

/** @brief The simulated accelerometer. */
#define SIM_ACCEL (&pbl_accel_sim)

/**
 * @brief Queue samples for replay.
 *
 * Samples that do not fit in the queue are dropped.
 *
 * @param accel Device.
 * @param samples Samples.
 * @param count Number of samples.
 * @return Number of samples queued.
 */
size_t pbl_accel_sim_feed(const struct pbl_accel *accel, const struct pbl_accel_sim_sample *samples,
                          size_t count);

/**
 * @brief Drop the queued samples, keeping the last one replayed latched.
 *
 * @param accel Device.
 */
void pbl_accel_sim_flush(const struct pbl_accel *accel);

/**
 * @brief Report an event, as if the part detected it.
 *
 * Dropped while detection of that event type is disabled, as on real hardware.
 *
 * @param accel Device.
 * @param event Event.
 */
void pbl_accel_sim_report_event(const struct pbl_accel *accel, const struct pbl_accel_event *event);

/** @} */
