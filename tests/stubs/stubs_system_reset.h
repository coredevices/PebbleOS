/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/passert.h"

#include "stubs_passert.h"

void system_reset_prepare(bool skip_bt_teardown) {}

void system_reset(void) {
  PBL_ASSERT(false, "System reset triggered!");
}

void system_reset_callback(void *callback) {}

