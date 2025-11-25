/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"
#include "util/build_id.h"
#include "system/status_codes.h"

#include <stdbool.h>

//! NOTE: This function performs a hard reset after the core dump and never returns
NORETURN core_dump_reset(bool is_forced);

bool is_unread_coredump_available(void);

// Used for unit tests
void core_dump_test_force_bus_fault(void);
void core_dump_test_force_inf_loop(void);
void core_dump_test_force_assert(void);


// Warning: these functions use the normal flash driver
status_t core_dump_size(uint32_t flash_base, uint32_t *size);
void core_dump_mark_read(uint32_t flash_base);
bool core_dump_is_unread_available(uint32_t flash_base);
uint32_t core_dump_get_slot_address(unsigned int i);

// Bluetooth Core Dump API
bool core_dump_reserve_ble_slot(uint32_t *flash_base, uint32_t *max_size,
                                ElfExternalNote *build_id);
