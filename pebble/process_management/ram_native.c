/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The worker and app segments, as plain objects, where no linker script lays
// them out.

#define PRV_STR(x)  #x
#define PRV_XSTR(x) PRV_STR(x)
#define PRV_SYM(s)  PRV_XSTR(__USER_LABEL_PREFIX__) #s

#define PRV_REGION(_start, _end, _size)           \
  char _start[_size] __attribute__((aligned(8))); \
  __asm__(                                        \
      ".globl " PRV_SYM(_end) "\n.set " PRV_SYM(_end) ", " PRV_SYM(_start) " + " PRV_XSTR(_size))

#define PRV_WORKER_RAM_SIZE (CONFIG_WORKER_RAM_SEGMENT_SIZE + CONFIG_WORKER_RAM_RUNTIME_SIZE)
#define PRV_APP_RAM_SIZE    (CONFIG_APP_RAM_4X_SEGMENT_SIZE + CONFIG_APP_RAM_4X_RUNTIME_SIZE)

PRV_REGION(__WORKER_RAM__, __WORKER_RAM_end__, PRV_WORKER_RAM_SIZE);
PRV_REGION(__APP_RAM__, __APP_RAM_end__, PRV_APP_RAM_SIZE);
