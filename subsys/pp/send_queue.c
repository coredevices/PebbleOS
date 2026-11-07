/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <pbl/kernel/assert.h>
#include <pbl/util/math.h>

static struct pbl_pp_send_job *prv_next(const struct pbl_pp_send_job *job) {
  return (struct pbl_pp_send_job *)job->node.next;
}

void pbl_pp_send_job_submit(struct pbl_pp_session *session, struct pbl_pp_send_job **job) {
  bool is_open;

  pbl_pp__lock();
  is_open = pbl_pp__session_is_open(session);
  if (is_open) {
    PBL_ASSERTN(!list_contains((ListNode *)session->send_queue, &(*job)->node));
    if (session->send_queue != nullptr) {
      list_append(&session->send_queue->node, &(*job)->node);
    } else {
      session->send_queue = *job;
    }
  } else {
    (*job)->api->free(*job);
    *job = nullptr;
  }
  pbl_pp__unlock();

  if (is_open) {
    pbl_pp_session_send_next(session);
  }
}

void pbl_pp__send_queue_drop(struct pbl_pp_session *session) {
  struct pbl_pp_send_job *job = session->send_queue;

  pbl_pp__assert_locked();

  while (job != nullptr) {
    struct pbl_pp_send_job *next = prv_next(job);

    job->api->free(job);
    job = next;
  }

  session->send_queue = nullptr;
}

size_t pbl_pp_session_send_queue_length(const struct pbl_pp_session *session) {
  size_t length = 0U;

  pbl_pp__lock();
  for (const struct pbl_pp_send_job *job = session->send_queue; job != nullptr;
       job = prv_next(job)) {
    length += job->api->get_length(job);
  }
  pbl_pp__unlock();

  return length;
}

size_t pbl_pp_session_send_queue_copy(struct pbl_pp_session *session, size_t offset, size_t length,
                                      uint8_t *data_out) {
  size_t remaining = length;

  pbl_pp__lock();
  for (const struct pbl_pp_send_job *job = session->send_queue; job != nullptr && remaining > 0U;
       job = prv_next(job)) {
    const size_t job_length = job->api->get_length(job);

    if (job_length <= offset) {
      offset -= job_length;
      continue;
    }

    const size_t copied = job->api->copy(job, offset, remaining, data_out);

    remaining -= copied;
    data_out += copied;
    offset = 0U;
  }
  pbl_pp__unlock();

  return length - remaining;
}

size_t pbl_pp_session_send_queue_peek(const struct pbl_pp_session *session,
                                      const uint8_t **data_out) {
  size_t length = 0U;

  pbl_pp__lock();
  if (session->send_queue != nullptr) {
    length = session->send_queue->api->get_read_pointer(session->send_queue, data_out);
  }
  pbl_pp__unlock();

  return length;
}

void pbl_pp_session_send_queue_consume(struct pbl_pp_session *session, size_t length) {
  pbl_pp__lock();
  PBL_ASSERTN(session->send_queue != nullptr);

  struct pbl_pp_send_job *job = session->send_queue;

  while (job != nullptr && length > 0U) {
    const size_t job_length = job->api->get_length(job);
    const size_t consumed = MIN(length, job_length);
    struct pbl_pp_send_job *next = prv_next(job);

    job->api->consume(job, consumed);
    if (consumed == job_length) {
      list_remove(&job->node, (ListNode **)&session->send_queue, nullptr);
      job->api->free(job);
    }

    length -= consumed;
    job = next;
  }
  pbl_pp__unlock();
}
