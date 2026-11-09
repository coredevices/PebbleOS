/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/syscall.h>
#include <pbl/profiler/profiler.h>

PBL_SYSCALL(void, sys_profiler_init) {
#ifdef CONFIG_PROFILER
  pbl_profiler_reset();
#endif
}

PBL_SYSCALL(void, sys_profiler_start) {
#ifdef CONFIG_PROFILER
  pbl_profiler_start();
#endif
}

PBL_SYSCALL(void, sys_profiler_stop) {
#ifdef CONFIG_PROFILER
  pbl_profiler_stop();
#endif
}

PBL_SYSCALL(void, sys_profiler_print_stats) {
#ifdef CONFIG_PROFILER
  pbl_profiler_print_stats();
#endif
}
