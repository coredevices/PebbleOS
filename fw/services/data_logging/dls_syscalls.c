/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdckdint.h>

#include <pbl/logging/logging.h>
#include <pbl/services/data_logging/data_logging_service.h>
#include <pbl/services/data_logging/dls_private.h>
#include <pbl/util/uuid.h>

#include <syscall/syscall.h>
#include <syscall/syscall_define.h>

PBL_LOG_MODULE_DECLARE(service_data_logging, CONFIG_SERVICE_DATA_LOGGING_LOG_LEVEL);

static void prv_assert_session_owned_by_caller(DataLoggingSession *session) {
  const PebbleProcessMd *md = sys_process_manager_get_current_process_md();
  if (md == nullptr || !uuid_equal(&session->app_uuid, &md->uuid)) {
    syscall_failed();
  }
}

PBL_SYSCALL(DataLoggingSessionRef, sys_data_logging_create, (VAL, uint32_t, tag),
            (VAL, DataLoggingItemType, item_type), (VAL, uint16_t, item_size),
            (VAL, void *, buffer), (VAL, bool, resume)) {
  // Apps allocate the circular buffer themselves in their own heap and hand the
  // pointer to the kernel via this syscall. Without validation, a malicious app
  // could point at kernel memory and turn dls_log into an arbitrary kernel write.
  if (from_user) {
    if (buffer != nullptr) {
      syscall_assert_userspace_buffer(buffer, DLS_SESSION_MIN_BUFFER_SIZE);
    } else if (pebble_task_get_current() != PebbleTask_Worker) {
      syscall_failed();
    }
  }
  return dls_create_current_process(tag, item_type, item_size, buffer, resume);
}

PBL_SYSCALL(void, sys_data_logging_finish, (VAL, DataLoggingSessionRef, session_ref)) {
  // dls_is_session_valid() (below) walks the kernel-owned s_logging_sessions
  // list to confirm the session pointer is one we handed out, so a forged
  // session_ref simply returns invalid and never reaches dls_finish().
  DataLoggingSession *session = (DataLoggingSession *)session_ref;

  if (!dls_is_session_valid(session)) {
    PBL_LOG_WRN("finish: Invalid session %p", session);
    return; // TODO: Return error code?
  }

  if (from_user) {
    prv_assert_session_owned_by_caller(session);
  }

  dls_finish(session);
}

PBL_SYSCALL(DataLoggingResult, sys_data_logging_log, (VAL, DataLoggingSessionRef, session_ref),
            (VAL, const void *, data), (VAL, uint32_t, num_items)) {
  DataLoggingSession *session = (DataLoggingSession *)session_ref;

  if (!dls_is_session_valid(session)) {
    PBL_LOG_WRN("log: Invalid session %p", session);
    return DATA_LOGGING_INVALID_PARAMS;
  }
  if (data == nullptr) {
    PBL_LOG_WRN("log: NULL data pointer");
    return DATA_LOGGING_INVALID_PARAMS;
  }

  if (from_user) {
    prv_assert_session_owned_by_caller(session);

    uint32_t num_bytes;
    if (ckd_mul(&num_bytes, num_items, (uint32_t)session->item_size)) {
      syscall_failed();
    }
    syscall_assert_userspace_buffer(data, num_bytes);
  }

  return dls_log(session, data, num_items);
}
