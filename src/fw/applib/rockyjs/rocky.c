/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rocky.h"

#include "jmem-heap.h"

#include "applib/rockyjs/api/rocky_api.h"
#include "applib/rockyjs/api/rocky_api_util.h"
#include "applib/rockyjs/pbl_jerry_port.h"
#include "applib/app.h"
#include "applib/applib_resource_private.h"
#include "applib/app_heap_analytics.h"
#include "applib/app_heap_util.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_manager.h"
#include "system/passert.h"
#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"

const RockySnapshotHeader ROCKY_EXPECTED_SNAPSHOT_HEADER = {
  .signature = {'P', 'J', 'S', 0}, // C-string terminator in case somebody treats this as source
#if CAPABILITY_HAS_ROCKY_JS
  .version = (uint8_t)CAPABILITY_JAVASCRIPT_BYTECODE_VERSION,
#endif
};

static void prv_rocky_init(void) {
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);
  rocky_api_watchface_init();
}

bool rocky_is_snapshot(const uint8_t *buffer, size_t buffer_size) {
#if CAPABILITY_HAS_ROCKY_JS
  const size_t header_length = sizeof(ROCKY_EXPECTED_SNAPSHOT_HEADER);
  if (buffer_size < header_length ||
      memcmp(ROCKY_EXPECTED_SNAPSHOT_HEADER.signature,
             buffer,
             sizeof(ROCKY_EXPECTED_SNAPSHOT_HEADER.signature)) != 0) {
    return false;
  }

  const uint8_t actual_version = buffer[offsetof(RockySnapshotHeader, version)];
  const uint8_t expected_version = ROCKY_EXPECTED_SNAPSHOT_HEADER.version;
  if (expected_version != actual_version) {
    PBL_LOG(LOG_LEVEL_WARNING, "incompatible JS snapshot version %"PRIu8" (expected: %"PRIu8")",
            actual_version, expected_version);
    return false;
  }

  return jerry_is_snapshot(buffer + header_length, buffer_size - header_length);
#else
  return false;
#endif
}

static bool prv_rocky_eval_buffer(const uint8_t *buffer, size_t buffer_size) {
  jerry_value_t rv;
  if (rocky_is_snapshot(buffer, buffer_size)) {
    buffer += sizeof(ROCKY_EXPECTED_SNAPSHOT_HEADER);
    buffer_size -= sizeof(ROCKY_EXPECTED_SNAPSHOT_HEADER);
    PBL_ASSERTN((uintptr_t)buffer % 8 == 0);
    rv = jerry_exec_snapshot(buffer, buffer_size, false);
  } else {
    PBL_LOG(LOG_LEVEL_INFO, "Not a snapshot, interpreting buffer as JS source code");
    rv = jerry_eval((jerry_char_t *) buffer, buffer_size, false);
  }

  bool error_occurred = jerry_value_has_error_flag(rv);
  if (error_occurred) {
    jerry_value_clear_error_flag(&rv);
    rocky_log_exception("Evaluating JS", rv);
  }

  jerry_release_value(rv);
  return !(error_occurred);
}

static void prv_rocky_deinit(void) {
  app_heap_analytics_log_stats_to_app_heartbeat(true /* is_rocky_app */);
  rocky_api_deinit();
  jerry_cleanup();
  rocky_runtime_context_deinit();
}

bool rocky_event_loop_with_string_or_snapshot(const void *buffer, size_t buffer_size) {
#if CAPABILITY_HAS_ROCKY_JS
  prv_rocky_init();
  const bool result = prv_rocky_eval_buffer(buffer, buffer_size);
  if (result) {
    app_event_loop_common();
  }
  prv_rocky_deinit();

  return result;
#else
  return false;
#endif
}

static bool prv_rocky_event_loop_with_resource(ResAppNum app_num, uint32_t resource_id) {
#if CAPABILITY_HAS_ROCKY_JS
  if (!sys_get_current_app_is_rocky_app()) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Cannot execute JavaScript, insufficient meta data.");
    return false;
  }

  bool rv = false;
  size_t sz = sys_resource_size(app_num, resource_id);
  char *script = applib_resource_mmap_or_load(app_num,
                                              resource_id,
                                              0, sz, true);
  if (script) {
    // TODO: PBL-40010 clean this up
    // hotfix: we're either dealing with mmap, which is 8 byte aligned already
    // or malloc`ed buffer which has 7 additional bytes at the end.
    // We're are moving over the bytes so that they are 8-byte aligned
    // and pass that pointer to rocky instead
    char *aligned_script = (char *)((uintptr_t)(script + 7) & ~7);
    if (aligned_script != script) {
      // don't write if it's aligned, to avoid writing to mmapped data
      memmove(aligned_script, script, sz);
    }

    rv = rocky_event_loop_with_string_or_snapshot(aligned_script, sz);
    applib_resource_munmap_or_free(script);
  }

  return rv;
#else
  return false;
#endif
}

bool rocky_event_loop_with_system_resource(uint32_t resource_id) {
  return prv_rocky_event_loop_with_resource(SYSTEM_APP, resource_id);
}

bool rocky_event_loop_with_resource(uint32_t resource_id) {
  return prv_rocky_event_loop_with_resource(sys_get_current_resource_num(),
                                            resource_id);
}
