/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/comm_session/session.h>
#include <pbl/util/list.h>

#include <stdint.h>

/**
 * @defgroup services_comm_session_session_send_queue Send queue
 * @ingroup services_comm_session
 * @brief Per-session queue of outbound jobs, each holding complete messages.
 * @{
 */

/** @brief Send queue job, see struct SessionSendQueueJob. */
typedef struct SessionSendQueueJob SessionSendQueueJob;

/**
 * @brief Callbacks implementing a send queue job.
 *
 * All callbacks are called with bt_lock() held.
 */
typedef struct {
  /**
   * @brief Get the remaining length of the job.
   *
   * @param send_job The job.
   * @return Length of the unconsumed message data in bytes.
   */
  size_t (*get_length)(const SessionSendQueueJob *send_job);

  /**
   * @brief Copy message data out of the job.
   *
   * The caller ensures enough data is available.
   *
   * @param send_job The job.
   * @param start_offset Offset into the unconsumed data.
   * @param length Number of bytes to copy.
   * @param[out] data_out Destination buffer.
   * @return Number of bytes copied.
   */
  size_t (*copy)(const SessionSendQueueJob *send_job, int start_offset, size_t length,
                 uint8_t *data_out);

  /**
   * @brief Get a pointer to the next contiguous chunk of data.
   *
   * The data may be non-contiguous: call this and consume() repeatedly until it returns 0 to
   * access all of it.
   *
   * @param send_job The job.
   * @param[out] data_out Read pointer.
   * @return Number of bytes readable at @p data_out.
   */
  size_t (*get_read_pointer)(const SessionSendQueueJob *send_job, const uint8_t **data_out);

  /**
   * @brief Mark data as sent by the transport.
   *
   * @param send_job The job.
   * @param length Number of bytes consumed.
   */
  void (*consume)(const SessionSendQueueJob *send_job, size_t length);

  /**
   * @brief Release the job.
   *
   * Called when the job has been fully consumed or the session is closed.
   *
   * @param send_job The job.
   */
  void (*free)(SessionSendQueueJob *send_job);
} SessionSendJobImpl;

/**
 * @brief Job sending one or more complete Pebble Protocol messages.
 *
 * Embed it as the first member of a larger structure to carry job-specific context.
 */
typedef struct SessionSendQueueJob {
  /** Node in the session's send queue. */
  ListNode node;

  /** Job implementation. */
  const SessionSendJobImpl *impl;
} SessionSendQueueJob;

/**
 * @brief Append a job to a session's send queue and schedule sending.
 *
 * The caller keeps the job alive until @c impl->free() is called. If the session is no longer
 * valid, the job is freed immediately and @p *job is set to NULL. bt_lock() need not be held.
 *
 * @param session Destination session.
 * @param[in,out] job Job to enqueue.
 */
void comm_session_send_queue_add_job(CommSession *session, SessionSendQueueJob **job);

/** @} */
