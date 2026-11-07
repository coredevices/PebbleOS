/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <errno.h>
#include <string.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>

PBL_LOG_MODULE_DECLARE(pp, CONFIG_PP_LOG_LEVEL);

#define MAX_PAYLOAD ((size_t)1024)

// Per class, excluding sizeof(struct pbl_pp_send_buffer) so that its layout does not change how
// many small messages fit.
#define MAX_BYTES (sizeof(struct pbl_pp_header) + MAX_PAYLOAD)

// On top of either class's limit, so interactive messages still fit while bulk transfers fill
// theirs.
#define RESERVE_BYTES ((size_t)256)

enum pbl_pp__send_class : uint8_t {
  CLASS_NORMAL,
  CLASS_BULK,
  CLASS_COUNT,
};

struct pbl_pp_send_buffer {
  // First, so that buffers and jobs convert into each other. The session is valid until
  // pbl_pp_send_buffer_end(), the job afterwards.
  union {
    struct pbl_pp_session *session;
    struct pbl_pp_send_job job;
  };
  size_t capacity;
  size_t written;
  size_t consumed;
  enum pbl_pp__send_class cls;
  struct PBL_PACKED {
    struct pbl_pp_header header;
    uint8_t payload[];
  };
};

// Given when a buffer is freed, one per class so that a retrying bulk sender cannot take the
// wakeup an interactive one waits for. Never taken with the session lock held.
static PBL_SEM_DEFINE(s_sem, 0, 1);
static PBL_SEM_DEFINE(s_bulk_sem, 0, 1);

// Locked.
static size_t s_allocated[CLASS_COUNT];

static struct pbl_sem *prv_sem(enum pbl_pp__send_class cls) {
  return (cls == CLASS_BULK) ? &s_bulk_sem : &s_sem;
}

static uint32_t prv_remaining_ms(uint32_t timeout_ms, uint64_t start_ticks) {
  const uint32_t elapsed_ms = ((pbl_uptime_ticks() - start_ticks) * 1000U) / PBL_TICK_HZ;

  return (timeout_ms > elapsed_ms) ? (timeout_ms - elapsed_ms) : 0U;
}

static struct pbl_pp_send_buffer *prv_create(struct pbl_pp_session *session, uint16_t endpoint_id,
                                             size_t capacity, enum pbl_pp__send_class cls) {
  const size_t message_size = sizeof(struct pbl_pp_header) + capacity;
  const size_t class_after = s_allocated[cls] + message_size;
  const size_t total_after = s_allocated[CLASS_NORMAL] + s_allocated[CLASS_BULK] + message_size;
  struct pbl_pp_send_buffer *sb;

  pbl_pp__assert_locked();

  if (class_after > MAX_BYTES || total_after > MAX_BYTES + RESERVE_BYTES) {
    return nullptr;
  }

  s_allocated[cls] = class_after;

  sb = pbl_zalloc_check(sizeof(*sb) + capacity);
  sb->session = session;
  sb->capacity = capacity;
  sb->cls = cls;
  sb->header.endpoint_id = pbl_cpu_to_be16(endpoint_id);

  return sb;
}

static size_t prv_remaining(const struct pbl_pp_send_buffer *sb) {
  return sizeof(struct pbl_pp_header) + sb->written - sb->consumed;
}

static const uint8_t *prv_read_pointer(const struct pbl_pp_send_buffer *sb) {
  return (const uint8_t *)&sb->header + sb->consumed;
}

static size_t prv_job_get_length(const struct pbl_pp_send_job *job) {
  return prv_remaining((const struct pbl_pp_send_buffer *)job);
}

static size_t prv_job_copy(const struct pbl_pp_send_job *job, size_t offset, size_t length,
                           uint8_t *data_out) {
  const struct pbl_pp_send_buffer *sb = (const struct pbl_pp_send_buffer *)job;
  const size_t copied = MIN(prv_remaining(sb) - offset, length);

  memcpy(data_out, prv_read_pointer(sb) + offset, copied);

  return copied;
}

static size_t prv_job_get_read_pointer(const struct pbl_pp_send_job *job,
                                       const uint8_t **data_out) {
  const struct pbl_pp_send_buffer *sb = (const struct pbl_pp_send_buffer *)job;

  *data_out = prv_read_pointer(sb);

  return prv_remaining(sb);
}

static void prv_job_consume(const struct pbl_pp_send_job *job, size_t length) {
  ((struct pbl_pp_send_buffer *)job)->consumed += length;
}

static void prv_job_free(struct pbl_pp_send_job *job) {
  struct pbl_pp_send_buffer *sb = (struct pbl_pp_send_buffer *)job;

  pbl_pp__assert_locked();

  s_allocated[sb->cls] -= sizeof(struct pbl_pp_header) + sb->capacity;
  pbl_free(sb);
  pbl_sem_give(&s_sem);
  pbl_sem_give(&s_bulk_sem);
}

static const struct pbl_pp_send_job_api s_job_api = {
  .get_length = prv_job_get_length,
  .copy = prv_job_copy,
  .get_read_pointer = prv_job_get_read_pointer,
  .consume = prv_job_consume,
  .free = prv_job_free,
};

size_t pbl_pp_send_buffer_max_payload(const struct pbl_pp_session *session) {
  size_t max_payload = 0U;

  pbl_pp__lock();
  if (pbl_pp__session_is_open(session)) {
    max_payload = MAX_PAYLOAD;
  }
  pbl_pp__unlock();

  return max_payload;
}

static struct pbl_pp_send_buffer *prv_begin(struct pbl_pp_session *session, uint16_t endpoint_id,
                                            size_t length, uint32_t timeout_ms,
                                            enum pbl_pp__send_class cls) {
  const uint64_t start_ticks = pbl_uptime_ticks();

  if (session == nullptr) {
    return nullptr;
  }

  if (length > MAX_PAYLOAD) {
    PBL_LOG_WRN("Message for endpoint_id %u exceeds maximum length (length=%" PRIu32 ")",
                endpoint_id, (uint32_t)length);
    return nullptr;
  }

  for (;;) {
    struct pbl_pp_send_buffer *sb;

    pbl_pp__lock();
    if (!pbl_pp__session_is_open(session)) {
      pbl_pp__unlock();
      return nullptr;
    }
    sb = prv_create(session, endpoint_id, length, cls);
    pbl_pp__unlock();

    if (sb != nullptr) {
      return sb;
    }

    const uint32_t remaining_ms = prv_remaining_ms(timeout_ms, start_ticks);

    if (remaining_ms == 0U) {
      break;
    }

    if (pbl_pp__in_send_task()) {
      // The send_next callback is queued behind us: nothing frees room unless we send now.
      pbl_pp__send_next_now(session);
    } else if (pbl_sem_take(prv_sem(cls), PBL_TICKS(remaining_ms)) != 0) {
      break;
    }
  }

  PBL_LOG_WRN("Failed to get send buffer (bytes=%" PRIu32 ", endpoint_id=%" PRIu16 ")",
              (uint32_t)length, endpoint_id);

  return nullptr;
}

struct pbl_pp_send_buffer *pbl_pp_send_buffer_begin(struct pbl_pp_session *session,
                                                    uint16_t endpoint_id, size_t length,
                                                    uint32_t timeout_ms) {
  return prv_begin(session, endpoint_id, length, timeout_ms, CLASS_NORMAL);
}

struct pbl_pp_send_buffer *pbl_pp_send_buffer_begin_bulk(struct pbl_pp_session *session,
                                                         uint16_t endpoint_id, size_t length,
                                                         uint32_t timeout_ms) {
  return prv_begin(session, endpoint_id, length, timeout_ms, CLASS_BULK);
}

bool pbl_pp_send_buffer_write(struct pbl_pp_send_buffer *sb, const uint8_t *data, size_t length) {
  if (PBL_UNLIKELY(sb->capacity - sb->written < length)) {
    return false;
  }

  memcpy(&sb->payload[sb->written], data, length);
  sb->written += length;

  return true;
}

void pbl_pp_send_buffer_end(struct pbl_pp_send_buffer *sb) {
  struct pbl_pp_session *session = sb->session;
  struct pbl_pp_send_job *job;

  sb->job = (struct pbl_pp_send_job){.api = &s_job_api};
  sb->header.length = pbl_cpu_to_be16(sb->written);

  job = &sb->job;
  pbl_pp_send_job_submit(session, &job);
}

int pbl_pp_send(struct pbl_pp_session *session, uint16_t endpoint_id, const uint8_t *data,
                size_t length, uint32_t timeout_ms) {
  struct pbl_pp_send_buffer *sb;

  if (pbl_pp_send_buffer_max_payload(session) == 0U) {
    return -ENOTCONN;
  }
  if (length > MAX_PAYLOAD) {
    return -EMSGSIZE;
  }

  sb = pbl_pp_send_buffer_begin(session, endpoint_id, length, timeout_ms);
  if (sb == nullptr) {
    PBL_LOG_WRN("Could not acquire send buffer for %x", endpoint_id);
    return -EAGAIN;
  }

  pbl_pp_send_buffer_write(sb, data, length);
  pbl_pp_send_buffer_end(sb);

  return 0;
}

#if UNITTEST
struct pbl_sem *pbl_pp__send_buffer_semaphore(void) {
  return &s_sem;
}

struct pbl_sem *pbl_pp__send_buffer_bulk_semaphore(void) {
  return &s_bulk_sem;
}

void pbl_pp__send_buffer_reset(void) {
  pbl_sem_reset(&s_sem);
  pbl_sem_reset(&s_bulk_sem);
}
#endif
