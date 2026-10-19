/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/** @cond INTERNAL_HIDDEN */

// What third_party/moddable still uses; see <pbl/kernel/syscall.h>.

#include <pbl/kernel/syscall.h>

#define mcu_call_unprivileged pbl_user_call

/** @endcond */
