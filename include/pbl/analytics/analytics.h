/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/section.h>

/**
 * @defgroup analytics Analytics
 * @ingroup subsys
 * @brief Firmware metrics collected into periodic heartbeats.
 *
 * Metrics are declared in @c analytics.def as unsigned, signed, scaled, timer or string metrics.
 * Values accumulate over a heartbeat period (one hour) and are then handed to the analytics
 * backends, which log them and reset them for the next period. Running timers keep running
 * across heartbeats. Without @c CONFIG_ANALYTICS the macros only evaluate their value.
 *
 * The functions are syscalls, so they may be used from apps as well as from the kernel. Keys out
 * of range are ignored.
 *
 * Metrics that are only worth computing once per period are refreshed by a collector, which runs
 * right before each heartbeat on the system task.
 *
 * @code{.c}
 * PBL_ANALYTICS_ADD(button_pressed_count, 1);
 *
 * PBL_ANALYTICS_TIMER_START(backlight_on_time_ms);
 * ...
 * PBL_ANALYTICS_TIMER_STOP(backlight_on_time_ms);
 *
 * static void prv_collect_pfs_stats(void) {
 *   PBL_ANALYTICS_SET_UNSIGNED(pfs_space_free_kb, get_available_pfs_space() / 1024);
 * }
 * PBL_ANALYTICS_COLLECTOR_DEFINE(prv_collect_pfs_stats);
 * @endcode
 * @{
 */

/**
 * @brief Key of a metric declared in @c analytics.def.
 *
 * @param key_name Metric name.
 */
#define PBL_ANALYTICS_KEY(key_name) PBL_ANALYTICS_KEY__##key_name

/** @brief Metric keys, one per entry in @c analytics.def. */
enum pbl_analytics_key {
/** @cond INTERNAL_HIDDEN */
#define PBL_ANALYTICS_METRIC_DEFINE_UNSIGNED(key)               PBL_ANALYTICS_KEY(key),
#define PBL_ANALYTICS_METRIC_DEFINE_SIGNED(key)                 PBL_ANALYTICS_KEY(key),
#define PBL_ANALYTICS_METRIC_DEFINE_SCALED_UNSIGNED(key, scale) PBL_ANALYTICS_KEY(key),
#define PBL_ANALYTICS_METRIC_DEFINE_SCALED_SIGNED(key, scale)   PBL_ANALYTICS_KEY(key),
#define PBL_ANALYTICS_METRIC_DEFINE_TIMER(key)                  PBL_ANALYTICS_KEY(key),
#define PBL_ANALYTICS_METRIC_DEFINE_STRING(key, len)            PBL_ANALYTICS_KEY(key),
#include "analytics.def"
#undef PBL_ANALYTICS_METRIC_DEFINE_UNSIGNED
#undef PBL_ANALYTICS_METRIC_DEFINE_SIGNED
#undef PBL_ANALYTICS_METRIC_DEFINE_SCALED_UNSIGNED
#undef PBL_ANALYTICS_METRIC_DEFINE_SCALED_SIGNED
#undef PBL_ANALYTICS_METRIC_DEFINE_TIMER
#undef PBL_ANALYTICS_METRIC_DEFINE_STRING
  /** @endcond */
  /** Number of metric keys. */
  PBL_ANALYTICS_KEY_COUNT,
};

/**
 * @brief Bits of the @c drv_init_fail_flags metric.
 *
 * Only the first heartbeat after boot carries them.
 */
enum pbl_analytics_drv_init_fail_flag {
  /** The heart rate monitor driver failed to initialize. */
  PBL_ANALYTICS_DRV_INIT_FAIL_HRM = 1U << 0U,
};

/** @brief A function that refreshes metrics right before each heartbeat. */
struct pbl_analytics_collector {
  /** Function called on the system task. */
  void (*collect)(void);
};

/**
 * @brief Initialize the backends and start the hourly heartbeat timer.
 */
void pbl_analytics_init(void);

/**
 * @brief Set a signed metric.
 *
 * @param key Metric key.
 * @param signed_value New value.
 */
void sys_pbl_analytics_set_signed(enum pbl_analytics_key key, int32_t signed_value);

/**
 * @brief Set an unsigned metric.
 *
 * @param key Metric key.
 * @param unsigned_value New value.
 */
void sys_pbl_analytics_set_unsigned(enum pbl_analytics_key key, uint32_t unsigned_value);

/**
 * @brief Set a string metric.
 *
 * The string is copied and truncated to the length declared for the metric. Strings passed from
 * an app must lie in the app's memory region.
 *
 * @param key Metric key.
 * @param value NUL-terminated string.
 */
void sys_pbl_analytics_set_string(enum pbl_analytics_key key, const char *value);

/**
 * @brief Start a timer metric.
 *
 * Has no effect if the timer is already running.
 *
 * @param key Metric key.
 */
void sys_pbl_analytics_timer_start(enum pbl_analytics_key key);

/**
 * @brief Stop a timer metric, adding the elapsed time in milliseconds to it.
 *
 * Has no effect if the timer is not running.
 *
 * @param key Metric key.
 */
void sys_pbl_analytics_timer_stop(enum pbl_analytics_key key);

/**
 * @brief Add to an integer metric.
 *
 * @param key Metric key.
 * @param amount Amount to add, may be negative.
 */
void sys_pbl_analytics_add(enum pbl_analytics_key key, int32_t amount);

#ifdef CONFIG_ANALYTICS

/**
 * @brief Set a signed metric by name.
 *
 * @param key_name Metric name.
 * @param signed_value New value.
 */
#define PBL_ANALYTICS_SET_SIGNED(key_name, signed_value) \
  sys_pbl_analytics_set_signed(PBL_ANALYTICS_KEY(key_name), signed_value)

/**
 * @brief Set an unsigned metric by name.
 *
 * @param key_name Metric name.
 * @param unsigned_value New value.
 */
#define PBL_ANALYTICS_SET_UNSIGNED(key_name, unsigned_value) \
  sys_pbl_analytics_set_unsigned(PBL_ANALYTICS_KEY(key_name), unsigned_value)

/**
 * @brief Set a string metric by name.
 *
 * @param key_name Metric name.
 * @param value NUL-terminated string.
 */
#define PBL_ANALYTICS_SET_STRING(key_name, value) \
  sys_pbl_analytics_set_string(PBL_ANALYTICS_KEY(key_name), value)

/**
 * @brief Start a timer metric by name.
 *
 * @param key_name Metric name.
 */
#define PBL_ANALYTICS_TIMER_START(key_name) \
  sys_pbl_analytics_timer_start(PBL_ANALYTICS_KEY(key_name))

/**
 * @brief Stop a timer metric by name.
 *
 * @param key_name Metric name.
 */
#define PBL_ANALYTICS_TIMER_STOP(key_name) sys_pbl_analytics_timer_stop(PBL_ANALYTICS_KEY(key_name))

/**
 * @brief Add to an integer metric by name.
 *
 * @param key_name Metric name.
 * @param amount Amount to add, may be negative.
 */
#define PBL_ANALYTICS_ADD(key_name, amount) \
  sys_pbl_analytics_add(PBL_ANALYTICS_KEY(key_name), amount)

/**
 * @brief Define a collector.
 *
 * @param _collect Function to call, an identifier unique among collectors.
 */
#ifdef PBL_NO_LINKER_SCRIPT
#define PBL_ANALYTICS_COLLECTOR_DEFINE(_collect)                                          \
  static const struct pbl_analytics_collector pbl_analytics_collector_##_collect PBL_USED \
  PBL_UNSORTED_SECTION(pbl_analytics) = {.collect = (_collect)}
#else
#define PBL_ANALYTICS_COLLECTOR_DEFINE(_collect)                                          \
  static const struct pbl_analytics_collector pbl_analytics_collector_##_collect PBL_USED \
      PBL_ALIGNED(4) PBL_SECTION(".pbl_analytics_collectors." #_collect) = {.collect = (_collect)}
#endif

#else

#define PBL_ANALYTICS_SET_SIGNED(key_name, signed_value)     ((void)(signed_value))
#define PBL_ANALYTICS_SET_UNSIGNED(key_name, unsigned_value) ((void)(unsigned_value))
#define PBL_ANALYTICS_SET_STRING(key_name, value)            ((void)(value))
#define PBL_ANALYTICS_TIMER_START(key_name)                  ((void)0)
#define PBL_ANALYTICS_TIMER_STOP(key_name)                   ((void)0)
#define PBL_ANALYTICS_ADD(key_name, amount)                  ((void)(amount))
#define PBL_ANALYTICS_COLLECTOR_DEFINE(_collect)               \
  [[maybe_unused]] static const struct pbl_analytics_collector \
      pbl_analytics_collector_##_collect = {.collect = (_collect)}

#endif /* CONFIG_ANALYTICS */

/** @} */
