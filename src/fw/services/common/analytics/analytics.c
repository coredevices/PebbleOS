/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>

#include "pbl/services/common/analytics/analytics.h"
#include "pbl/services/common/analytics/backend.h"
#include "pbl/services/common/new_timer/new_timer.h"
#include "pbl/services/common/system_task.h"
#include "system/version.h"
#include "util/size.h"

#define HEARTBEAT_PERIOD_SEC 3600

extern void pbl_analytics_external_collect_battery(void);
extern void pbl_analytics_external_collect_cpu_stats(void);
extern void pbl_analytics_external_collect_stack_free(void);
extern void pbl_analytics_external_collect_pfs_stats(void);
extern void pbl_analytics_external_collect_kernel_heap_stats(void);
extern void pbl_analytics_external_collect_backlight_stats(void);
extern void pbl_analytics_external_collect_vibe_stats(void);
extern void pbl_analytics_external_collect_settings(void);

#ifdef ANALYTICS_NATIVE
extern void pbl_analytics__native_init(void);
extern void pbl_analytics__native_heartbeat(void);

extern const struct pbl_analytics_backend_ops pbl_analytics__native_ops;
#endif

#ifdef ANALYTICS_MEMFAULT
extern void pbl_analytics__memfault_init(void);
extern void pbl_analytics__memfault_heartbeat(void);

extern const struct pbl_analytics_backend_ops pbl_analytics__memfault_ops;
#endif

static TimerID s_heartbeat_timer;

static void (*const s_init[])(void) = {
#ifdef ANALYTICS_NATIVE
    pbl_analytics__native_init,
#endif
#ifdef ANALYTICS_MEMFAULT
    pbl_analytics__memfault_init,
#endif
};

static void (*const s_heartbeat[])(void) = {
#ifdef ANALYTICS_NATIVE
    pbl_analytics__native_heartbeat,
#endif
#ifdef ANALYTICS_MEMFAULT
    pbl_analytics__memfault_heartbeat,
#endif
};

static const struct pbl_analytics_backend_ops *s_backend_ops[] = {
#ifdef ANALYTICS_NATIVE
    &pbl_analytics__native_ops,
#endif
#ifdef ANALYTICS_MEMFAULT
    &pbl_analytics__memfault_ops,
#endif
};

static void prv_heartbeat_system_task_cb(void *data) {
  PBL_ANALYTICS_SET_STRING(fw_version, TINTIN_METADATA.version_tag);

  pbl_analytics_external_collect_battery();
  pbl_analytics_external_collect_cpu_stats();
  pbl_analytics_external_collect_stack_free();
  pbl_analytics_external_collect_pfs_stats();
  pbl_analytics_external_collect_kernel_heap_stats();
  pbl_analytics_external_collect_backlight_stats();
  pbl_analytics_external_collect_vibe_stats();
  pbl_analytics_external_collect_settings();

  for (size_t i = 0U; i < ARRAY_LENGTH(s_heartbeat); i++) {
    s_heartbeat[i]();
  }
}

static void prv_heartbeat_timer_cb(void *data) {
  system_task_add_callback(prv_heartbeat_system_task_cb, NULL);
}

void pbl_analytics_init(void) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_init); i++) {
    s_init[i]();
  }

  s_heartbeat_timer = new_timer_create();

  new_timer_start(s_heartbeat_timer, HEARTBEAT_PERIOD_SEC * 1000, prv_heartbeat_timer_cb, NULL,
                  TIMER_START_FLAG_REPEATING);
}

void pbl_analytics_set_signed(enum pbl_analytics_key key, int32_t signed_value) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_backend_ops); i++) {
    s_backend_ops[i]->set_signed(key, signed_value);
  }
}

void pbl_analytics_set_unsigned(enum pbl_analytics_key key, uint32_t unsigned_value) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_backend_ops); i++) {
    s_backend_ops[i]->set_unsigned(key, unsigned_value);
  }
}

void pbl_analytics_set_string(enum pbl_analytics_key key, const char *value) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_backend_ops); i++) {
    s_backend_ops[i]->set_string(key, value);
  }
}

void pbl_analytics_timer_start(enum pbl_analytics_key key) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_backend_ops); i++) {
    s_backend_ops[i]->timer_start(key);
  }
}
void pbl_analytics_timer_stop(enum pbl_analytics_key key) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_backend_ops); i++) {
    s_backend_ops[i]->timer_stop(key);
  }
}

void pbl_analytics_add(enum pbl_analytics_key key, int32_t amount) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_backend_ops); i++) {
    s_backend_ops[i]->add(key, amount);
  }
}