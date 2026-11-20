/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/ram.h>
#include <pbl/kernel/time.h>
#include <pbl/util/math.h>

#define CHECK_LEN (1024U * 1024U)

static uint32_t prv_pattern(size_t i) {
  return (uint32_t)i * 2654435761U;
}

int main(void) {
  volatile uint32_t *mem;
  uintptr_t base;
  size_t size;
  size_t words;
  size_t bad = 0U;
  uint64_t start;
  int ret;

  /* The memory is only powered while it has users: become one. */
  ret = pbl_ram_get(EXTRAM);
  if (ret != 0) {
    fprintf(stderr, "ram: power on failed (%d)\n", ret);
    return ret;
  }

  pbl_ram_get_region(EXTRAM, &base, &size);
  printf("ram: %d KiB\n", (int)(size / 1024U));
  printf("ram: mapped at 0x%08x\n", (unsigned int)base);

  /* Memory-mapped: plain loads and stores. Its contents are undefined until written. */
  mem = (volatile uint32_t *)base;
  words = PBL_MIN(CHECK_LEN, size) / sizeof(uint32_t);

  start = pbl_uptime_ms();
  for (size_t i = 0U; i < words; i++) {
    mem[i] = prv_pattern(i);
  }
  for (size_t i = 0U; i < words; i++) {
    if (mem[i] != prv_pattern(i)) {
      bad++;
    }
  }
  printf("ram: wrote and read back %d KiB in %d ms\n", (int)(words * sizeof(uint32_t) / 1024U),
         (int)(pbl_uptime_ms() - start));

  /* Without users, the next deep sleep powers it off. */
  pbl_ram_put(EXTRAM);

  if (bad != 0U) {
    fprintf(stderr, "ram: %d words wrong\n", (int)bad);
    return -EIO;
  }

  printf("ram: all words match\n");
  printf("Sample done\n");

  return 0;
}
