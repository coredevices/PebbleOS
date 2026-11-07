/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <stdint.h>
#include <string.h>

#include <pbl/kernel/init.h>
#include <pbl/kernel/malloc.h>
#include <pbl/logging/backend.h>
#include <pbl/logging/logging.h>
#include <pbl/version.h>

static struct pbl_thread s_main_thread;
PBL_THREAD_STACK_DEFINE(s_main_stack, CONFIG_KERNEL_MAIN_STACK_SIZE);

static void prv_main_entry(void *arg) {
  int (*entry)(void) = arg;
  entry();
}

void kernel_init(int (*entry)(void)) {
  mpu_init();
  pbl_soc_early_init();
  pbl_irq_init();
  arch_init();
  pbl_malloc_init();
  pbl_log_early_init();

  PBL_LOG_ALWAYS("*** Booting PebbleOS %s ***", PBL_VERSION_TAG);

  const struct pbl_thread_attr attr = {
    .name = "KernelMain",
    .entry = prv_main_entry,
    .arg = (void *)entry,
    .prio = CONFIG_KERNEL_MAIN_THREAD_PRIO,
    .privileged = true,
    .stack = s_main_stack,
    .stack_size = sizeof(s_main_stack),
  };
  PBL_ASSERTN(pbl_thread_create(&s_main_thread, &attr) == 0);

  pbl_kernel_start();
}

#ifndef CONFIG_ARCH_POSIX
extern uint8_t __data_load_start[];
extern uint8_t __data_start[];
extern uint8_t __data_end[];
extern uint8_t __bss_start[];
extern uint8_t __bss_end[];

#ifdef CONFIG_RAMFUNC
extern uint8_t __ramfunc_load_start[];
extern uint8_t __ramfunc_start[];
extern uint8_t __ramfunc_end[];
#endif

extern int main(void);

void kernel_prep_c(void) {
  memcpy(__data_start, __data_load_start, __data_end - __data_start);
#ifdef CONFIG_RAMFUNC
  memcpy(__ramfunc_start, __ramfunc_load_start, __ramfunc_end - __ramfunc_start);
#endif
  memset(__bss_start, 0, __bss_end - __bss_start);

  kernel_init(main);
}
#endif
