/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl_jcontext.inc.h"
#include "jerry-port.h"

#include "applib/app_heap_analytics.h"
#include "applib/app_logging.h"
#include "applib/pbl_std/pbl_std.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"
#include "system/logging.h"
#include "system/passert.h"

#include <stdarg.h>
#include <stdint.h>

static uint8_t prv_pbl_log_level_from_jerry_log_level(const jerry_log_level_t level) {
  switch (level) {
    case JERRY_LOG_LEVEL_ERROR:
      return LOG_LEVEL_ERROR;
    case JERRY_LOG_LEVEL_WARNING:
      return LOG_LEVEL_WARNING;
    case JERRY_LOG_LEVEL_TRACE:
      return LOG_LEVEL_DEBUG_VERBOSE;
    case JERRY_LOG_LEVEL_DEBUG:
    default:
      return LOG_LEVEL_DEBUG;
      break;
  }
}

/**
 * Provide log message implementation for the engine.
 */
void jerry_port_log(jerry_log_level_t level, const char* format, ...) {
  const uint8_t log_level = prv_pbl_log_level_from_jerry_log_level(level);
  if (log_level > LOG_LEVEL_DEBUG) {
    return;
  }

  va_list args;
  va_start(args, format);
  app_log_vargs(log_level, "JERRY-LOG", 0, format, args);
  va_end(args);
}

/**
 * Provide console message implementation for the engine.
 */
void jerry_port_console(const char *format, ...) {
  if (format[0] == '\n' && strlen(format) == 1) {
    return;
  }
  va_list args;
  va_start(args, format);
  app_log_vargs(LOG_LEVEL_DEBUG, "JERRY-CONSOLE", 0, format, args);
  va_end(args);
}

void jerry_port_fatal(jerry_fatal_code_t code, void *lr) {
  if (ERR_OUT_OF_MEMORY == code) {
    app_heap_analytics_log_rocky_heap_oom_fault();
  }

  jerry_port_log(JERRY_LOG_LEVEL_ERROR, "Fatal Error: %d", code);
  PBL_ASSERTN_LR(false, (uint32_t)lr);
}

RockyRuntimeContext * rocky_runtime_context_get(void) {
  return app_state_get_rocky_runtime_context();
}

#define ALIGNED_HEAP(ptr) (void *)JERRY_ALIGNUP((uintptr_t)(ptr), JMEM_ALIGNMENT)

void rocky_runtime_context_init(void) {
  uint8_t *unaligned_buffer = task_zalloc(sizeof(RockyRuntimeContext) + JMEM_ALIGNMENT);
  RockyRuntimeContext * const ctx = ALIGNED_HEAP(unaligned_buffer);
  app_state_set_rocky_runtime_context(unaligned_buffer, ctx);
}

void rocky_runtime_context_deinit(void) {
  task_free(app_state_get_rocky_runtime_context_buffer());
  app_state_set_rocky_runtime_context(NULL, NULL);
}

double jerry_port_get_current_time (void) {
  time_t seconds;
  uint16_t millis;
  time_ms(&seconds, &millis);
  return ((double)seconds * 1000.0) + millis;
} /* jerry_port_get_current_time */

DEFINE_SYSCALL(bool, jerry_port_get_time_zone, jerry_time_zone_t *tz_p) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(tz_p, sizeof(*tz_p));
  }

  time_t utc_now;
  time_ms(&utc_now, NULL);
  int32_t dstoffset = time_get_isdst(utc_now) ? time_get_dstoffset() : 0;
  tz_p->daylight_saving_time = dstoffset / 3600;
  tz_p->offset = -1 * time_get_gmtoffset() / 60;

  return true;
}
