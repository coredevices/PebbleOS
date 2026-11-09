/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/i18n/i18n.h>
#include <pbl/kernel/compiler.h>

#include <applib/i18n.h>

const char *PBL_WEAK pbl_i18n_get(const char *msgid, const void *owner) {
  // If a string wasn't found, we want to return the original string.
  // However, if we have a context, this string needs to not show the context.
  // So we just find EOT and if it's present return the next character.
  const char *message = strchr(msgid, '\4');
  if (message == nullptr) {
    // No context, the whole string is the message.
    return msgid;
  }
  // strchr gets the address of that character. We want to skip the EOT, so +1.
  return message + 1;
}

void PBL_WEAK pbl_i18n_get_with_buffer(const char *string, char *buffer, size_t length) {
  strncpy(buffer, pbl_i18n_get(string, (void *)0x1234), length);
  buffer[length - 1] = '\0';
  pbl_i18n_free(string, (void *)0x1234);
}

size_t PBL_WEAK pbl_i18n_get_length(const char *string) {
  size_t size = strlen(pbl_i18n_get(string, (void *)0x1234));
  pbl_i18n_free(string, (void *)0x1234);
  return size;
}

void PBL_WEAK pbl_i18n_free(const char *original, const void *owner) {
}

void PBL_WEAK pbl_i18n_free_all(const void *owner) {
}

void PBL_WEAK sys_i18n_get_with_buffer(const char *string, char *buffer, size_t length) {
  pbl_i18n_get_with_buffer(string, buffer, length);
}

size_t PBL_WEAK sys_i18n_get_length(const char *string) {
  return pbl_i18n_get_length(string);
}

void PBL_WEAK sys_i18n_get_locale(char *buf) {
  strncpy(buf, "en_US", PBL_I18N_LOCALE_LEN);
}

const char *PBL_WEAK app_get_system_locale(void) {
  return "en_US";
}
