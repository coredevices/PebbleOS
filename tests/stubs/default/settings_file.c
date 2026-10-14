/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/settings/settings_file.h>

PBL_WEAK status_t settings_file_open(SettingsFile *file, const char *name, int max_used_space) {
  return S_SUCCESS;
}

PBL_WEAK status_t settings_file_open_growable(SettingsFile *file, const char *name,
                                              int max_used_space, int initial_alloc_size) {
  return settings_file_open(file, name, max_used_space);
}

PBL_WEAK status_t settings_file_get(SettingsFile *file, const void *key, size_t key_len,
                                    void *val_out, size_t val_out_len) {
  return S_SUCCESS;
}

PBL_WEAK status_t settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                                    const void *val, size_t val_len) {
  return S_SUCCESS;
}

PBL_WEAK status_t settings_file_each(SettingsFile *file, SettingsFileEachCallback cb,
                                     void *context) {
  return S_SUCCESS;
}

PBL_WEAK int settings_file_get_len(SettingsFile *file, const void *key, size_t key_len) {
  return 0;
}

PBL_WEAK status_t settings_file_delete(SettingsFile *file, const void *key, size_t key_len) {
  return S_SUCCESS;
}

PBL_WEAK status_t settings_file_compact(SettingsFile *file) {
  return S_SUCCESS;
}

PBL_WEAK void settings_file_close(SettingsFile *file) {
}
