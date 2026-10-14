/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

PBL_WEAK bool firmware_update_is_in_progress(void) {
  return false;
}
