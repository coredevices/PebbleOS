/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/comm_session/session.h"
#include "util/list.h"

#include <stdint.h>

typedef struct SessionSendQueueJob SessionSendQueueJob;

//! @note bt_lock() is expected to be taken by the caller of any of these functions!
typedef struct {
  //! @return The size of the message(s) of this job in bytes.
  size_t (*get_length)(const SessionSendQueueJob *send_job);

  //! Copies bytes from the message(s) into another buffer.
  //! @param start_off The offset into the send buffer
  //! @param length The number of bytes to copy
  //! @param[out] data_out Pointer to the buffer into which to copy the data
  //! @return The number of bytes copied
  //! @note The caller will ensure there is enough data available.
  size_t (*copy)(const SessionSendQueueJob *send_job, int start_offset,
                 size_t length, uint8_t *data_out);

  //! Gets a read pointer and the number of bytes that can be read from the read pointer.
  //! @note The implementation might use a non-contiguous buffer, so it is possible
  //! that there is more data to read. To access the entire message data, call this function
  //! and consume() repeatedly until it returns zero.
  //! @param data_out Pointer to the pointer to assign the read pointer to.
  //! @return The number of bytes that can be read starting at the read pointer.
  size_t (*get_read_pointer)(const SessionSendQueueJob *send_job,
                             const uint8_t **data_out);

  //! Indicates that `length` bytes have been consumed and sent out by the transport.
  void (*consume)(const SessionSendQueueJob *send_job, size_t length);

  //! Called when the send queue is done consuming the job, or when the session is disconnected
  //! and the job should clean itself up.
  void (*free)(SessionSendQueueJob *send_job);
} SessionSendJobImpl;

//! Structure representing a job to send one or more complete Pebble Protocol messages.
//! Future possibility: add a priority level, so the jobs can be sent out in priority order.
typedef struct SessionSendQueueJob {
  ListNode node;

  //! Job implementation
  const SessionSendJobImpl *impl;

  //! The creator of the job can potentially tack more context fields to the end here.
} SessionSendQueueJob;

//! The caller is responsible for keeping around the job until impl->free() is called.
//! @note If the session has been closed in the mean time, impl->free() will be called before
//! returning from this function. In that case, job will be set to NULL.
//! bt_lock() does not have to be held by the caller.
void comm_session_send_queue_add_job(CommSession *session, SessionSendQueueJob **job);
