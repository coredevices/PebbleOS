/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup soc_sf32lb SF32LB
 * @ingroup soc
 * @brief SiFli SF32LB52 SoC interfaces.
 *
 * The idle thread enters the deepest sleep level that no driver has blocked. A driver whose
 * peripheral would not survive a level blocks it while active:
 *
 * @code{.c}
 * soc_sf32lb_sleep_block(SOC_SF32LB_DEEPSLEEP); // the PWM does not run in deep sleep
 * prv_pwm_start();
 * prv_pwm_wait_done();
 * prv_pwm_stop();
 * soc_sf32lb_sleep_release(SOC_SF32LB_DEEPSLEEP);
 * @endcode
 * @{
 */

/** @brief Sleep levels, ordered from shallowest to deepest. */
typedef enum {
  /** No sleep at all. */
  SOC_SF32LB_ACTIVE = 0,
  /** Light WFI. */
  SOC_SF32LB_WFI,
  /** Deep WFI. */
  SOC_SF32LB_DEEPWFI,
  /** Deep sleep. */
  SOC_SF32LB_DEEPSLEEP,
} SocSf32lbSleepLevel;

/**
 * @brief Block a sleep level and every deeper one.
 *
 * For example, blocking @ref SOC_SF32LB_DEEPWFI forbids deep WFI and deep sleep, leaving plain
 * WFI as the deepest permitted level. Refcounted; balance each call with
 * soc_sf32lb_sleep_release(). Safe to call concurrently, including from ISRs.
 *
 * @param level Level to block; @ref SOC_SF32LB_ACTIVE cannot be blocked.
 */
void soc_sf32lb_sleep_block(SocSf32lbSleepLevel level);

/**
 * @brief Release a block taken with soc_sf32lb_sleep_block().
 *
 * @param level Level passed to soc_sf32lb_sleep_block().
 */
void soc_sf32lb_sleep_release(SocSf32lbSleepLevel level);

/**
 * @brief Get the deepest sleep level currently permitted.
 *
 * @return One step shallower than the shallowest outstanding block, or
 *         @ref SOC_SF32LB_DEEPSLEEP with no blocks.
 */
SocSf32lbSleepLevel soc_sf32lb_sleep_max_level(void);

#ifdef CONFIG_DEMO_APP_BT_CLASSIC_PROBE
#include <stdint.h>

//! RTC ticks spent in each sleep since boot, and the times idle found the IPC queue busy since
//! the last analytics heartbeat; for the BT Classic Probe.
void soc_sf32lb_sleep_totals(uint64_t *wfi, uint64_t *deepwfi, uint64_t *deepsleep,
                             uint32_t *ipc_not_idle);

//! Idles since boot that could not deep sleep: a driver blocked it, or the next wake was nearer
//! than the minimum window.
void soc_sf32lb_sleep_why(uint32_t *blocked, uint32_t *short_window);

#define SOC_SF32LB_WAKE_PINS 45

//! What woke the deep sleeps since boot: the wake timer, the LCPU, each GPIO
//! pin (PA00 on), anything else.
void soc_sf32lb_sleep_wakes(uint32_t *timer, uint32_t *lcpu, uint32_t *other,
                            uint32_t pins[SOC_SF32LB_WAKE_PINS]);

//! Deep sleeps since boot by length: under 0.2 s, under 1 s, under 5 s, longer.
void soc_sf32lb_sleep_lengths(uint32_t lengths[4]);

//! Outstanding blocks of @p level.
int soc_sf32lb_sleep_blocks(SocSf32lbSleepLevel level);
#endif

/** @} */
