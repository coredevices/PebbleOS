/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/phone_call.h>
#include <pbl/services/phone_call_util.h>

PBL_WEAK PebblePhoneCaller *phone_call_util_create_caller(const char *number, const char *name) {
  return nullptr;
}

PBL_WEAK void phone_call_util_destroy_caller(PebblePhoneCaller *caller) {
}
