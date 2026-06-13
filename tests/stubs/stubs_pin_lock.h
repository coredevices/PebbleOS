/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/pin_lock/pin_lock.h"
#include "util/attributes.h"

bool WEAK pin_lock_is_locked(void) {
  return false;
}

bool WEAK pin_lock_should_hide_notifications(void) {
  return false;
}

bool WEAK pin_lock_should_hide_timeline(void) {
  return false;
}
