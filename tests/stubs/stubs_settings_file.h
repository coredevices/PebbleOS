/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/settings/settings_file.h"

status_t settings_file_open(SettingsFile *file, const char *name, int max_used_space) {
  return S_SUCCESS;
}

status_t settings_file_get(SettingsFile *file, const void *key, size_t key_len,
                           void *val_out, size_t val_out_len) {
  return S_SUCCESS;
}

status_t settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                           const void *val, size_t val_len) {
  return S_SUCCESS;
}

status_t settings_file_each(SettingsFile *file, SettingsFileEachCallback cb, void *context) {
  return S_SUCCESS;
}

int settings_file_get_len(SettingsFile *file, const void *key, size_t key_len) {
  return 0;
}

status_t settings_file_delete(SettingsFile *file, const void *key, size_t key_len) {
  return S_SUCCESS;
}

void settings_file_close(SettingsFile *file) {}
