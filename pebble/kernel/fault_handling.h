/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stddef.h>
#include <stdint.h>

#include <system/reboot_reason.h>

[[noreturn]] void trigger_fault(RebootReasonCode reason_code, uint32_t lr);
