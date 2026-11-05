/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/idle.h>
#include <pbl/kernel/init.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/sched.h>
#include <pbl/util/size.h>

#include <kernel/util/idle.h>
#include <pbl_arch_posix.h>
#include <posix_host.h>

uint32_t SystemCoreClock = 64000000;

#ifdef CONFIG_ARCH_POSIX_FIBERS
// One host thread: the main loop runs the kernel until it idles.
static void prv_kernel_poll(void) {
  pbl_posix_run();
}

POSIX_HOST_HOOK(.poll = prv_kernel_poll)
#endif

void pbl_soc_early_init(void) {
}

static void prv_wait(void) {
  pbl_posix_cpu_wait(UINT64_MAX);
}

const struct pbl_idle_state pbl_soc_idle_states[] = {
  {.name = "wait", .min_ticks = 1, .exit_ticks = 0, .enter = prv_wait},
};
const size_t pbl_soc_num_idle_states = ARRAY_LENGTH(pbl_soc_idle_states);

int pbl_soc_idle_state_limit(void) {
  return idle_is_allowed() ? 0 : -1;
}

[[noreturn]] void pbl_soc_reset(void) {
  posix_host_reboot();
}

void posix_fw_start(void) {
  extern int pbl_fw_main(void);
  pbl_posix_start(pbl_fw_main);
}
