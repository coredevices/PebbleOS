/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>

#include <pbl/analytics/analytics.h>
#include <pbl/analytics/backend.h>
#include <pbl/kernel/heap.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/syscall.h>
#include <pbl/kernel/work.h>
#include <pbl/services/time.h>
#include <pbl/util/size.h>

#include <system/reboot_reason.h>
#include <system/version.h>

#define HEARTBEAT_PERIOD_SEC     3600
#define ANALYTICS_STRING_MAX_LEN 64

#ifndef PBL_NO_LINKER_SCRIPT
extern const struct pbl_analytics_collector __pbl_analytics_collectors_start[];
extern const struct pbl_analytics_collector __pbl_analytics_collectors_end[];
#else
extern const struct pbl_analytics_collector
    __pbl_analytics_collectors_start[] PBL_UNSORTED_SECTION_START(pbl_analytics);
extern const struct pbl_analytics_collector
    __pbl_analytics_collectors_end[] PBL_UNSORTED_SECTION_END(pbl_analytics);
#endif

extern void pbl_analytics__native_init(void);
extern void pbl_analytics__native_heartbeat(void);

extern const struct pbl_analytics_backend_ops pbl_analytics__native_ops;

static void prv_heartbeat_timer_cb(struct pbl_work *work);

static PBL_DELAYED_WORK_DEFINE(s_heartbeat_timer, prv_heartbeat_timer_cb);

static void (*const s_init[])(void) = {
  pbl_analytics__native_init,
};

static void (*const s_heartbeat[])(void) = {
  pbl_analytics__native_heartbeat,
};

static const struct pbl_analytics_backend_ops *s_backend_ops[] = {
  &pbl_analytics__native_ops,
};

static void prv_collect_kernel_heap_stats(void) {
  struct pbl_heap *heap = pbl_malloc_heap();
  size_t total_size = pbl_heap_size(heap);
  size_t headroom = pbl_heap_min_headroom(heap);
  uint32_t headroom_pct = (total_size > 0) ? (headroom * 100) / total_size : 0;
  PBL_ANALYTICS_SET_UNSIGNED(memory_pct_max, headroom_pct);

  // Peak usage alone hides fragmentation
  unsigned int used, free_bytes, max_free;
  pbl_heap_stats(heap, &used, &free_bytes, &max_free);
  uint32_t largest_free_pct = (total_size > 0) ? ((uint32_t)max_free * 100) / total_size : 0;
  PBL_ANALYTICS_SET_UNSIGNED(memory_largest_free_pct, largest_free_pct);

  // Restart the peak so heavy periods stand out
  pbl_heap_reset_high_water_mark(heap);
}

static void prv_heartbeat_sys_work_cb(void *data) {
  static bool s_reboot_reason_counted = false;

  PBL_ANALYTICS_SET_STRING(fw_version, TINTIN_METADATA.version_tag);
  PBL_ANALYTICS_SET_UNSIGNED(last_reboot_reason, reboot_reason_get_last_reboot_reason());
  PBL_ANALYTICS_SET_UNSIGNED(uptime_s, time_get_uptime_seconds());

  if (!s_reboot_reason_counted) {
    s_reboot_reason_counted = true;
    if (reboot_reason_get_last_reboot_reason() >= RebootReasonCode_Watchdog) {
      PBL_ANALYTICS_ADD(unexpected_reboot_count, 1);
    }
  }

  prv_collect_kernel_heap_stats();

  for (const struct pbl_analytics_collector *c = __pbl_analytics_collectors_start;
       c < __pbl_analytics_collectors_end; c++) {
    c->collect();
  }

  for (size_t i = 0U; i < ARRAY_LENGTH(s_heartbeat); i++) {
    s_heartbeat[i]();
  }
}

static void prv_heartbeat_timer_cb(struct pbl_work *work) {
  pbl_sys_work_call(prv_heartbeat_sys_work_cb, nullptr);
  pbl_delayed_work_schedule_next(&s_heartbeat_timer, pbl_ms_to_ticks(HEARTBEAT_PERIOD_SEC * 1000));
}

void pbl_analytics_init(void) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_init); i++) {
    s_init[i]();
  }

  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_heartbeat_timer,
                              PBL_MSEC(HEARTBEAT_PERIOD_SEC * 1000));
}

// Apps pass `key` straight through to backends that use it as an unchecked
// index into fixed-size kernel tables (`s_key_to_integer[]`,
// `s_string_ptrs[]`, `s_string_lens[]`, ...). An out-of-range key reads
// arbitrary kernel bytes; in the set_string path those bytes become a kernel
// pointer + length that strncpy() then writes the (validated) app string into,
// turning analytics into an arbitrary kernel write primitive. Bound the key
// here once and let the backends keep their direct indexing.
static bool prv_analytics_key_in_range(enum pbl_analytics_key key) {
  return ((unsigned)key < PBL_ANALYTICS_KEY_COUNT);
}

PBL_SYSCALL(void, sys_pbl_analytics_set_signed, (VAL, enum pbl_analytics_key, key),
            (VAL, int32_t, signed_value)) {
  if (!prv_analytics_key_in_range(key)) {
    return;
  }
  for (size_t i = 0U; i < ARRAY_LENGTH(s_backend_ops); i++) {
    s_backend_ops[i]->set_signed(key, signed_value);
  }
}

PBL_SYSCALL(void, sys_pbl_analytics_set_unsigned, (VAL, enum pbl_analytics_key, key),
            (VAL, uint32_t, unsigned_value)) {
  if (!prv_analytics_key_in_range(key)) {
    return;
  }
  for (size_t i = 0U; i < ARRAY_LENGTH(s_backend_ops); i++) {
    s_backend_ops[i]->set_unsigned(key, unsigned_value);
  }
}

PBL_SYSCALL(void, sys_pbl_analytics_set_string, (VAL, enum pbl_analytics_key, key),
            (STRN, const char *, value, ANALYTICS_STRING_MAX_LEN)) {
  if (!prv_analytics_key_in_range(key)) {
    return;
  }

  for (size_t i = 0U; i < ARRAY_LENGTH(s_backend_ops); i++) {
    s_backend_ops[i]->set_string(key, value);
  }
}

PBL_SYSCALL(void, sys_pbl_analytics_timer_start, (VAL, enum pbl_analytics_key, key)) {
  if (!prv_analytics_key_in_range(key)) {
    return;
  }
  for (size_t i = 0U; i < ARRAY_LENGTH(s_backend_ops); i++) {
    s_backend_ops[i]->timer_start(key);
  }
}

PBL_SYSCALL(void, sys_pbl_analytics_timer_stop, (VAL, enum pbl_analytics_key, key)) {
  if (!prv_analytics_key_in_range(key)) {
    return;
  }
  for (size_t i = 0U; i < ARRAY_LENGTH(s_backend_ops); i++) {
    s_backend_ops[i]->timer_stop(key);
  }
}

PBL_SYSCALL(void, sys_pbl_analytics_add, (VAL, enum pbl_analytics_key, key),
            (VAL, int32_t, amount)) {
  if (!prv_analytics_key_in_range(key)) {
    return;
  }
  for (size_t i = 0U; i < ARRAY_LENGTH(s_backend_ops); i++) {
    s_backend_ops[i]->add(key, amount);
  }
}

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>

static int prv_cmd_heartbeat(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_sys_work_call(prv_heartbeat_sys_work_cb, nullptr);
  return 0;
}

PBL_SHELL_SUBCMD_SET_CREATE(sub_analytics);
PBL_SHELL_CMD_REGISTER(analytics, sub_analytics, "Analytics", nullptr);
PBL_SHELL_SUBCMD_ADD(sub_analytics, heartbeat, nullptr, "Collect a heartbeat now",
                     prv_cmd_heartbeat, 0, 0);
#endif
