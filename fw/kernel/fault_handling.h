/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stddef.h>
#include <stdint.h>

#include <system/reboot_reason.h>

typedef struct Heap Heap;

PBL_NORETURN void trigger_oom_fault(size_t bytes, uint32_t lr, Heap *heap_ptr);
PBL_NORETURN void trigger_fault(RebootReasonCode reason_code, uint32_t lr);
void enable_fault_handlers(void);
bool fault_handling_hit_stack_guard(const unsigned int *stacked_args, unsigned int exc_return);
