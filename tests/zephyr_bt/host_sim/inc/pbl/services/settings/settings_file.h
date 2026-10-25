/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Stand-in for the firmware header, with what ctl/zbt_ctl.c uses.
#pragma once
#include <stdbool.h>
#include <stddef.h>

#include <system/status_codes.h>
typedef struct SettingsFile {
  int open;
} SettingsFile;
status_t settings_file_open(SettingsFile *file, const char *name, int max_used_space);
void settings_file_close(SettingsFile *file);
bool settings_file_exists(SettingsFile *file, const void *key, size_t key_len);
status_t settings_file_delete(SettingsFile *file, const void *key, size_t key_len);
int settings_file_get_len(SettingsFile *file, const void *key, size_t key_len);
status_t settings_file_get(SettingsFile *file, const void *key, size_t key_len, void *val_out,
                           size_t val_out_len);
status_t settings_file_set(SettingsFile *file, const void *key, size_t key_len, const void *val,
                           size_t val_len);
