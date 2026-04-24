/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef struct PebblePhoneCaller {
  char *number;
  char *name;
} PebblePhoneCaller;

//! Creates a new caller to pass as part of a phone event
//! @param number The phone number for this caller
//! @param name The name of the caller
//! @return Pointer to new caller
PebblePhoneCaller* phone_call_util_create_caller(const char *number, const char *name);

//! Destroys a caller previously created with \ref phone_call_util_create_caller
//! @param caller The caller to free
void phone_call_util_destroy_caller(PebblePhoneCaller *caller);
