/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/analytics/analytics.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/time.h>
#include <pbl/util/math.h>

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>
#endif

// CPU statistics
///////////////////////////////////////////////////////////

struct cpu_stats {
  uint64_t total_us;
  uint64_t idle_us[PBL_IDLE_STATES_MAX];
};

static struct cpu_stats s_last;

static void prv_snapshot(struct cpu_stats *stats) {
  stats->total_us = (pbl_uptime_ticks() * 1000000U) / PBL_TICK_HZ;
  for (size_t i = 0; i < PBL_IDLE_STATES_MAX; i++) {
    stats->idle_us[i] = pbl_idle_residency_us(i);
  }
}

// Time spent since the last collection.
static void prv_delta(struct cpu_stats *delta) {
  prv_snapshot(delta);
  delta->total_us -= s_last.total_us;
  for (size_t i = 0; i < PBL_IDLE_STATES_MAX; i++) {
    delta->idle_us[i] -= s_last.idle_us[i];
  }
}

static uint64_t prv_running_us(const struct cpu_stats *delta) {
  uint64_t idle_us = 0U;

  for (size_t i = 0; i < PBL_IDLE_STATES_MAX; i++) {
    idle_us += delta->idle_us[i];
  }

  return (delta->total_us > idle_us) ? (delta->total_us - idle_us) : 0U;
}

static uint16_t prv_pct100(uint64_t part, uint64_t total) {
  return (total > 0U) ? (uint16_t)MIN((part * 10000U) / total, 10000U) : 0U;
}

static void prv_collect_cpu_stats(void) {
  struct cpu_stats delta;

  prv_delta(&delta);

  PBL_ANALYTICS_SET_UNSIGNED(cpu_running_pct, prv_pct100(prv_running_us(&delta), delta.total_us));
  PBL_ANALYTICS_SET_UNSIGNED(cpu_sleep0_pct, prv_pct100(delta.idle_us[0], delta.total_us));
  PBL_ANALYTICS_SET_UNSIGNED(cpu_sleep1_pct, prv_pct100(delta.idle_us[1], delta.total_us));
  PBL_ANALYTICS_SET_UNSIGNED(cpu_sleep2_pct, prv_pct100(delta.idle_us[2], delta.total_us));

  prv_snapshot(&s_last);
}
PBL_ANALYTICS_COLLECTOR_DEFINE(prv_collect_cpu_stats);

#ifdef CONFIG_SHELL
static int prv_cmd_force_active(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_idle_set_enabled(false);
  return 0;
}

static int prv_cmd_resume_normal(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_idle_set_enabled(true);
  return 0;
}

static int prv_cmd_cpustats(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct cpu_stats delta;
  uint64_t running_us;

  prv_delta(&delta);
  running_us = prv_running_us(&delta);

  pbl_shell_print(sh, "%-10s %10" PRIu32 " ms (%" PRIu32 " %%)", "run",
                  (uint32_t)(running_us / 1000U),
                  (uint32_t)prv_pct100(running_us, delta.total_us) / 100U);
  for (size_t i = 0; i < pbl_soc_num_idle_states; i++) {
    pbl_shell_print(sh, "%-10s %10" PRIu32 " ms (%" PRIu32 " %%)", pbl_soc_idle_states[i].name,
                    (uint32_t)(delta.idle_us[i] / 1000U),
                    (uint32_t)prv_pct100(delta.idle_us[i], delta.total_us) / 100U);
  }
  pbl_shell_print(sh, "%-10s %10" PRIu32 " ms", "total", (uint32_t)(delta.total_us / 1000U));

  return 0;
}

PBL_SHELL_SUBCMD_SET_CREATE(sub_sys_scheduler);
PBL_SHELL_SUBCMD_ADD(sub_sys, scheduler, sub_sys_scheduler, "Scheduler idle control", nullptr, 0,
                     0);
PBL_SHELL_SUBCMD_ADD(sub_sys_scheduler, force_active, nullptr, "Keep the CPU out of idle",
                     prv_cmd_force_active, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_sys_scheduler, resume_normal, nullptr, "Allow the CPU to idle again",
                     prv_cmd_resume_normal, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_sys, cpustats, nullptr, "Show CPU time per idle state since collected",
                     prv_cmd_cpustats, 0, 0);
#endif
