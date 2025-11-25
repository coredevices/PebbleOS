/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/reboot_reason.h"
#include "util/attributes.h"

#if !UNITTEST
NORETURN
#else
void
#endif
enter_standby(RebootReasonCode reason);
