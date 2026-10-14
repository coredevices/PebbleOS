/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

PBL_WEAK void calendar_handle_pin_change(void) {
  return;
}

PBL_WEAK bool calendar_event_is_ongoing(void) {
  return false;
}
