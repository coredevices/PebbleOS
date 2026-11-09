/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/i18n/i18n.h>
#include <pbl/kernel/malloc.h>
#include <pbl/services/phone_call_util.h>

PebblePhoneCaller *phone_call_util_create_caller(const char *number, const char *name) {
  PebblePhoneCaller *caller = pbl_zalloc(sizeof(PebblePhoneCaller));
  if (!caller) {
    return nullptr;
  }

  if ((!name || !strlen(name)) && (!number || !strlen(number))) {
    const char *i18n_name = pbl_i18n_get("Unknown", __FILE__);

    caller->name = pbl_malloc(strlen(i18n_name) + 1);
    if (caller->name) {
      strcpy(caller->name, i18n_name);
    }
    pbl_i18n_free(i18n_name, __FILE__);

    return caller;
  }

  if (number) {
    caller->number = pbl_malloc(strlen(number) + 1);
    if (caller->number) {
      strcpy(caller->number, number);
    }
  }
  if (name) {
    caller->name = pbl_malloc(strlen(name) + 1);
    if (caller->name) {
      strcpy(caller->name, name);
    }
  }

  return caller;
}

void phone_call_util_destroy_caller(PebblePhoneCaller *caller) {
  if (caller) {
    pbl_free(caller->number);
    pbl_free(caller->name);
    pbl_free(caller);
  }
}
