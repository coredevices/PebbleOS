/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdint.h>

#include "pbl/util/build_id.h"

// What the linker script lays out on the target, as plain objects.

#define PRV_STR(x)  #x
#define PRV_XSTR(x) PRV_STR(x)
#define PRV_SYM(s)  PRV_XSTR(__USER_LABEL_PREFIX__) #s

// A region the firmware finds through a start and an end symbol.
#define PRV_REGION(_start, _end, _size)           \
  char _start[_size] __attribute__((aligned(8))); \
  __asm__(                                        \
      ".globl " PRV_SYM(_end) "\n.set " PRV_SYM(_end) ", " PRV_SYM(_start) " + " PRV_XSTR(_size))

#define PRV_KERNEL_HEAP_SIZE 524288
#define PRV_WORKER_RAM_SIZE  12288
#define PRV_APP_RAM_SIZE     (CONFIG_APP_RAM_4X_SEGMENT_SIZE + CONFIG_APP_RAM_4X_RUNTIME_SIZE)

PRV_REGION(_heap_start, _heap_end, PRV_KERNEL_HEAP_SIZE);
PRV_REGION(__WORKER_RAM__, __WORKER_RAM_end__, PRV_WORKER_RAM_SIZE);
PRV_REGION(__APP_RAM__, __APP_RAM_end__, PRV_APP_RAM_SIZE);

uint32_t __kernel_main_stack_start__[4096 / sizeof(uint32_t)];
uint32_t __kernel_bg_stack_start__[4096 / sizeof(uint32_t)];
uint32_t __isr_stack_start__[1024 / sizeof(uint32_t)];

const struct {
  uint32_t name_length;
  uint32_t data_length;
  uint32_t type;
  uint8_t data[4 + BUILD_ID_EXPECTED_LEN];
} TINTIN_BUILD_ID = {
  .name_length = 4,
  .data_length = BUILD_ID_EXPECTED_LEN,
  .type = 3,
  .data = {'G', 'N', 'U', '\0'},
};

_Static_assert(offsetof(__typeof__(TINTIN_BUILD_ID), data) == offsetof(ElfExternalNote, data), "");
