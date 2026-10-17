/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>

PBL_WEAK void system_reset_prepare(void) {
}

PBL_WEAK void system_reset(void) {
  PBL_ASSERT(false, "System reset triggered!");
}

PBL_WEAK void system_reset_callback(void *callback) {
}
