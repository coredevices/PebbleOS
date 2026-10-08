/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <stdint.h>
#include <string.h>

#include <pbl/kernel/init.h>

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

  pbl_soc_early_init();

  main();

  KERNEL_ASSERT(false);
}
