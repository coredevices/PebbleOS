/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/i18n/i18n.h"
#include "util/attributes.h"

#include <string.h>

const char *WEAK i18n_get(const char *msgid, const void *owner) {
  // If a string wasn't found, we want to return the original string.
  // However, if we have a context, this string needs to not show the context.
  // So we just find EOT and if it's present return the next character.
  const char *message = strchr(msgid, '\4');
  if (message == NULL) {
    // No context, the whole string is the message.
    return msgid;
  }
  // strchr gets the address of that character. We want to skip the EOT, so +1.
  return message + 1;
}

void WEAK i18n_get_with_buffer(const char *string, char *buffer, size_t length) {
  strncpy(buffer, i18n_get(string, (void *)0x1234), length);
  buffer[length - 1] = '\0';
  i18n_free(string, (void *)0x1234);
}

size_t WEAK i18n_get_length(const char *string) {
  size_t size = strlen(i18n_get(string, (void *)0x1234));
  i18n_free(string, (void *)0x1234);
  return size;
}

void WEAK i18n_free(const char *original, const void *owner) {
}

void WEAK i18n_free_all(const void *owner) {
}

void WEAK sys_i18n_get_with_buffer(const char *string, char *buffer, size_t length) {
  i18n_get_with_buffer(string, buffer, length);
}

size_t WEAK sys_i18n_get_length(const char *string) {
  return i18n_get_length(string);
}

void WEAK i18n_enable(bool enable) { }
