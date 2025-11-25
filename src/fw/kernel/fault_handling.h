/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "system/reboot_reason.h"

typedef struct Heap Heap;

NORETURN trigger_oom_fault(size_t bytes, uint32_t lr, Heap *heap_ptr);
NORETURN trigger_fault(RebootReasonCode reason_code, uint32_t lr);
void enable_fault_handlers(void);

