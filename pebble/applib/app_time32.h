/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <time.h>

#include <pbl/util/time.h>

#include <syscall/syscall.h>

//! Widen a time an app passed in, relative to the current time.
static inline time_t app_time_from_time32(pbl_time32_t t) {
  return pbl_time_from_time32(t, sys_get_time());
}

//! Narrow a time handed back to an app, or return -1 if the app could not read it back.
static inline pbl_time32_t app_time_to_time32_or_error(time_t t) {
  const pbl_time32_t t32 = pbl_time_to_time32(t);
  if (pbl_time_from_time32(t32, sys_get_time()) != t) {
    return -1;
  }
  return t32;
}
