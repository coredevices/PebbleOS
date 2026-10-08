/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

#include <pbl/kernel/idle.h>
#include <pbl/kernel/init.h>
#include <pbl_arch_posix.h>

#include <kernel/util/idle.h>
#include <posix_host.h>

uint32_t SystemCoreClock = 64000000;

static uint64_t s_tick_start_ms;
static uint64_t s_ticks_announced;

static uint64_t prv_monotonic_ms(void) {
  return posix_host_monotonic_us() / 1000;
}

// Catches up on every tick that passed while the kernel held the CPU.
static void prv_tick_isr(void *arg) {
  uint64_t now = prv_monotonic_ms() - s_tick_start_ms;
  while (s_ticks_announced < now) {
    s_ticks_announced++;
    pbl_kernel_tick_isr();
  }
}

static void *prv_tick_thread(void *arg) {
  for (;;) {
    usleep(1000000 / PBL_TICK_HZ);
    pbl_posix_irq_run(prv_tick_isr, NULL);
  }
  return NULL;
}

void pbl_soc_early_init(void) {
  s_tick_start_ms = prv_monotonic_ms();
  pthread_t tid;
  pthread_create(&tid, NULL, prv_tick_thread, NULL);
}

void pbl_soc_idle(pbl_tick_t max_ticks) {
  if (!idle_is_allowed() || !pbl_idle_confirm()) {
    return;
  }
  pbl_posix_cpu_wait((uint64_t)max_ticks * 1000000 / PBL_TICK_HZ);
}

bool pbl_soc_tick_enable(void) {
  return true;
}

void pbl_analytics_external_collect_cpu_stats(void) {
}

static void *prv_firmware_thread(void *arg) {
  extern int pbl_fw_main(void);
  pbl_posix_boot(pbl_fw_main);
}

void posix_fw_start(void) {
  pthread_t tid;
  pthread_create(&tid, NULL, prv_firmware_thread, NULL);
}
