/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

bool PBL_WEAK bt_ctl_is_airplane_mode_on(void) {
  return false;
}
