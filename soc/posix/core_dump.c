/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>

#include <kernel/core_dump.h>
#include <kernel/core_dump_private.h>
#include <system/status_codes.h>

// No core dumps: a fatal error stops the process where a debugger or the
// host's crash reporter can look at it.

void coredump_assert(int line) {
  fprintf(stderr, "core dump assert at line %d\n", line);
  abort();
}

PBL_NORETURN void core_dump_reset(bool is_forced) {
  fprintf(stderr, "core dump requested, aborting\n");
  abort();
}

status_t core_dump_size(uint32_t flash_base, uint32_t *size) {
  return E_DOES_NOT_EXIST;
}

void core_dump_mark_read(uint32_t flash_base) {
}

bool core_dump_is_unread_available(uint32_t flash_base) {
  return false;
}

uint32_t core_dump_get_slot_address(unsigned int i) {
  return CORE_DUMP_FLASH_INVALID_ADDR;
}

bool core_dump_reserve_ble_slot(uint32_t *flash_base, uint32_t *max_size,
                                ElfExternalNote *build_id) {
  return false;
}

void core_dump_test_force_bus_fault(void) {
  abort();
}

void core_dump_test_force_inf_loop(void) {
  abort();
}

void core_dump_test_force_assert(void) {
  abort();
}

// Third-party apps are ARM binaries, which a native build cannot load.
const void *const g_pbl_system_tbl[] = {0};
