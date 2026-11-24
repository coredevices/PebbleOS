/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/sensor.h>

/**
 * @defgroup sensing Sensing
 * @ingroup subsys
 * @brief Shares sensors between clients.
 *
 * A sensor of the sensor class has a single listener. The sensing subsystem is that listener for
 * every sensor it manages, and shares the sensor between any number of clients, each asking for a
 * channel type at its own sampling interval and batch size:
 *
 * - The sensor samples at the shortest interval any client asks for, and queues frames for as
 *   long as the client with the shortest batch duration allows, up to the depth of its FIFO.
 * - Batches are stored once, as the driver produced them, in a buffer per sensor and type. Each
 *   client has its own position in it, and reads frames decoded to the canonical unit of the
 *   type, decimated to its own interval with a phase accumulator, taking the first frame at or
 * after each due time, so rates that do not divide the sampling rate are spread evenly. A client
 * that falls behind loses its oldest frames without holding back the others.
 * - A client arms a threshold with pbl_sensing_arm() and is notified once that many frames are
 *   waiting, after which it is disarmed until armed again.
 * - Event detection, such as shakes, is enabled while any client wants the event, and events go to
 *   every client that wants them.
 *
 * Clients' callbacks run on the high priority system work queue, from the sensor listener, with
 * the device lock held, as every sensing function takes it. They must not block, and may call
 * back into the subsystem.
 *
 * @code{.c}
 * static bool prv_notify(struct pbl_sensing_client *client) {
 *   int32_t ug[5][3];
 *   uint64_t t_us;
 *   int n = pbl_sensing_read(client, &ug[0][0], 5U, &t_us);
 *   ...
 *   pbl_sensing_arm(client, 5U);
 *   return true;
 * }
 *
 * static struct pbl_sensing_client s_client = {
 *   .sensor = ACCEL,
 *   .type = PBL_SENSOR_ACCEL,
 *   .interval_us = 40000,
 *   .batch = 5,
 *   .notify = prv_notify,
 * };
 *
 * pbl_sensing_client_add(&s_client);
 * pbl_sensing_arm(&s_client, 5U);
 * @endcode
 * @{
 */

struct pbl_sensing_client;

/** @cond INTERNAL_HIDDEN */
struct pbl_sensing_sensor;
struct pbl_sensing_stream;
/** @endcond */

/** @brief A client of a sensor. */
struct pbl_sensing_client {
  /** Sensor. */
  const struct pbl_sensor *sensor;
  /** Type of the frames to read. */
  enum pbl_sensor_type type;
  /** Sampling interval in microseconds, 0 for no frames. Change with
   * pbl_sensing_client_set_interval(). */
  uint32_t interval_us;
  /**
   * Frames per update at @c interval_us, bounding how long frames may queue in the sensor. 0
   * discards the frames, while the sensor still samples at @c interval_us. Change with
   * pbl_sensing_client_set_batch().
   */
  uint16_t batch;
  /** Events wanted, a bit per enum pbl_sensor_event_type. Change with
   * pbl_sensing_client_set_events(). */
  uint8_t events;
  /**
   * Optional. Called when the armed number of frames are waiting, with the client disarmed. Returns
   * true if the frames were handed over, false to stay armed and be called again on the next
   * batch.
   */
  bool (*notify)(struct pbl_sensing_client *client);
  /** Optional. Called on each wanted event, in the watch frame. */
  void (*event)(struct pbl_sensing_client *client, const struct pbl_sensor_event *event);
  /** Free for the client's use. */
  void *user_data;
  /** @cond INTERNAL_HIDDEN */
  struct pbl_sensing_client *next;
  struct pbl_sensing_sensor *ctx;
  struct pbl_sensing_stream *stream;
  uint32_t pos;
  uint32_t phase;
  uint32_t lost;
  uint16_t armed;
  /** @endcond */
};

/**
 * @brief Add a client.
 *
 * The client reads frames acquired from now on. The sensor is reconfigured for it, which may
 * report queued batches to the other clients first.
 *
 * @param client Client, with the fields up to @c user_data set, kept until removed.
 * @return 0, -EINVAL for an invalid client, -ENOTSUP if the sensor does not measure the type or
 *         detect the events, or -ENOMEM.
 */
int pbl_sensing_client_add(struct pbl_sensing_client *client);

/**
 * @brief Remove a client.
 *
 * Once it returns, the client's callbacks are not called anymore. A client that was not added, or
 * failed to, is ignored, as by the other functions taking a client.
 *
 * @param client Client.
 */
void pbl_sensing_client_remove(struct pbl_sensing_client *client);

/**
 * @brief Change the sampling interval of a client.
 *
 * @param client Client.
 * @param interval_us Sampling interval in microseconds, 0 for no frames.
 */
void pbl_sensing_client_set_interval(struct pbl_sensing_client *client, uint32_t interval_us);

/**
 * @brief Change the batch size of a client.
 *
 * @param client Client.
 * @param batch Frames per update, 0 to discard the frames.
 */
void pbl_sensing_client_set_batch(struct pbl_sensing_client *client, uint16_t batch);

/**
 * @brief Change the events a client wants.
 *
 * @param client Client.
 * @param events Events, a bit per enum pbl_sensor_event_type.
 */
void pbl_sensing_client_set_events(struct pbl_sensing_client *client, uint8_t events);

/**
 * @brief Get the interval the sensor samples a client's type at.
 *
 * @param client Client.
 * @return Interval in microseconds, 0 if not sampling.
 */
uint32_t pbl_sensing_sampling_interval(const struct pbl_sensing_client *client);

/**
 * @brief Notify the client once a number of frames are waiting.
 *
 * Does not notify for frames already waiting, only once a new batch brings the count up to
 * @p frames.
 *
 * @param client Client.
 * @param frames Frames, 0 to disarm.
 */
void pbl_sensing_arm(struct pbl_sensing_client *client, uint16_t frames);

/**
 * @brief Count the frames waiting for a client.
 *
 * @param client Client.
 * @return Frames, after decimation.
 */
uint32_t pbl_sensing_available(struct pbl_sensing_client *client);

/**
 * @brief Read the oldest frames waiting for a client.
 *
 * @param client Client.
 * @param[out] out Values in the canonical unit, pbl_sensor_num_values() per frame, in the watch
 *             frame for vectors.
 * @param max Maximum number of frames.
 * @param[out] timestamp_us Time of the first frame read in microseconds of uptime, or NULL.
 * @return Number of frames read.
 */
uint16_t pbl_sensing_read(struct pbl_sensing_client *client, int32_t *out, uint16_t max,
                          uint64_t *timestamp_us);

/**
 * @brief Discard the frames waiting for a client.
 *
 * @param client Client.
 */
void pbl_sensing_discard(struct pbl_sensing_client *client);

/**
 * @brief Get and clear the number of frames a client lost by falling behind.
 *
 * @param client Client.
 * @return Frames lost, before decimation, since the last call.
 */
uint32_t pbl_sensing_lost(struct pbl_sensing_client *client);

/**
 * @brief Get the latest frame of a type, without accessing the sensor.
 *
 * @param sensor Sensor.
 * @param type Type.
 * @param[out] val Values in the canonical unit, in the watch frame for vectors.
 * @param[out] timestamp_us Time of the frame in microseconds of uptime, or NULL.
 * @return 0, or -ENODATA if no client is reading frames of @p type, or none was acquired yet.
 */
int pbl_sensing_latest(const struct pbl_sensor *sensor, enum pbl_sensor_type type, int32_t *val,
                       uint64_t *timestamp_us);

/**
 * @brief Count the frames of a type acquired since the first client read them.
 *
 * @param sensor Sensor.
 * @param type Type.
 * @return Frames, wrapping around.
 */
uint32_t pbl_sensing_count(const struct pbl_sensor *sensor, enum pbl_sensor_type type);

/**
 * @brief Suspend or resume a sensor.
 *
 * While suspended, the sensor neither samples nor detects events, and clients are kept but get
 * nothing. Resuming restores what the clients want.
 *
 * @param sensor Sensor.
 * @param suspend True to suspend, false to resume.
 */
void pbl_sensing_suspend(const struct pbl_sensor *sensor, bool suspend);

/** @} */
