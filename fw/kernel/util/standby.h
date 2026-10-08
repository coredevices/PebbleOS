/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <system/reboot_reason.h>

#if !UNITTEST
PBL_NORETURN
#endif
void enter_standby(RebootReasonCode reason);
