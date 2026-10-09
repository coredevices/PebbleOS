/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <system/reboot_reason.h>

#if !UNITTEST
[[noreturn]]
#endif
void enter_standby(RebootReasonCode reason);
