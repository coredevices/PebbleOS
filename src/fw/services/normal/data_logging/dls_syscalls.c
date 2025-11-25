/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "data_logging_service.h"
#include "dls_private.h"

#include "kernel/memory_layout.h"
#include "syscall/syscall_internal.h"
#include "system/logging.h"

#include <inttypes.h>

DEFINE_SYSCALL(DataLoggingSessionRef, sys_data_logging_create, uint32_t tag,
               DataLoggingItemType item_type, uint16_t item_size,
               void *buffer, bool resume) {
  return dls_create_current_process(tag, item_type, item_size, buffer, resume);
}

DEFINE_SYSCALL(void, sys_data_logging_finish, DataLoggingSessionRef session_ref) {
  // TODO: It would be nice to verify the session itself, because they could be
  // passing us any memory address (not necesarilly a valid DataLoggingSession).
  // An evil developer could potentially use this to confuse the data_logging
  // logic, and do evil things with kernel rights. However, it's pretty unlikely
  // (especially since our executable code lives in microflash, and hence can't
  // just be overwritten by a buffer overrun), so it's probably fine.
  DataLoggingSession* session = (DataLoggingSession*)session_ref;

  if (!dls_is_session_valid(session)) {
    PBL_LOG(LOG_LEVEL_WARNING, "finish: Invalid session %p", session);
    return; // TODO: Return error code?
  }

  dls_finish(session);
}

DEFINE_SYSCALL(DataLoggingResult, sys_data_logging_log,
               DataLoggingSessionRef session_ref, void* data, uint32_t num_items) {
  DataLoggingSession* session = (DataLoggingSession*)session_ref;

  if (!dls_is_session_valid(session)) {
    PBL_LOG(LOG_LEVEL_WARNING, "log: Invalid session %p", session);
    return DATA_LOGGING_INVALID_PARAMS;
  }
  if (data == NULL) {
    PBL_LOG(LOG_LEVEL_WARNING, "log: NULL data pointer");
    return DATA_LOGGING_INVALID_PARAMS;
  }

  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(data, num_items * session->item_size);
  }

  return dls_log(session, data, num_items);
}
