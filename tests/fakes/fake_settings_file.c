/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include <pbl/crc/crc.h>
#include <pbl/services/settings/settings_file.h>

#include <clar_asserts.h>
#include <system/status_codes.h>

struct {
  bool open;
  void *values[UINT8_MAX];
  void *keys[UINT8_MAX];
  uint32_t val_lens[UINT8_MAX];
  uint32_t key_lens[UINT8_MAX];
  bool dirty[UINT8_MAX];
} s_settings_file;

void fake_settings_file_reset(void) {
  for (unsigned i = 0; i < UINT8_MAX; ++i) {
    free(s_settings_file.values[i]);
    s_settings_file.values[i] = nullptr;
    free(s_settings_file.keys[i]);
    s_settings_file.keys[i] = nullptr;
    s_settings_file.val_lens[i] = 0;
    s_settings_file.key_lens[i] = 0;
    s_settings_file.dirty[i] = false;
  }
  s_settings_file.open = false;
}

status_t settings_file_get(SettingsFile *file, const void *key, size_t key_len, void *val_out,
                           size_t val_out_len) {
  if (settings_file_exists(file, key, key_len)) {
    const uint8_t key_crc8 = pbl_crc8(0, key, key_len);
    memcpy(val_out, s_settings_file.values[key_crc8], val_out_len);
    return S_SUCCESS;
  }

  memset(val_out, 0, val_out_len);
  return E_DOES_NOT_EXIST;
}

status_t settings_file_set(SettingsFile *file, const void *key, size_t key_len, const void *val,
                           size_t val_len) {
  void *val_copy = malloc(val_len);
  void *key_copy = malloc(key_len);
  cl_assert(val_copy && key_copy);
  memcpy(val_copy, val, val_len);
  memcpy(key_copy, key, key_len);
  const uint8_t key_crc8 = pbl_crc8(0, key, key_len);
  if (s_settings_file.values[key_crc8] != nullptr) {
    cl_assert(memcmp(key, s_settings_file.keys[key_crc8], key_len) == 0);
  }

  free(s_settings_file.values[key_crc8]);
  free(s_settings_file.keys[key_crc8]);
  s_settings_file.values[key_crc8] = val_copy;
  s_settings_file.keys[key_crc8] = key_copy;
  s_settings_file.val_lens[key_crc8] = val_len;
  s_settings_file.key_lens[key_crc8] = key_len;
  s_settings_file.dirty[key_crc8] = true;
  return S_SUCCESS;
}

status_t settings_file_get_time(SettingsFile *file, const void *key, size_t key_len,
                                time_t *time_out) {
  *time_out = 0;
  const int len = settings_file_get_len(file, key, key_len);
  if (len == 0) {
    return E_DOES_NOT_EXIST;
  }
  if (len == sizeof(uint32_t)) {
    uint32_t t32;
    settings_file_get(file, key, key_len, &t32, sizeof(t32));
    *time_out = t32;
    return S_SUCCESS;
  }
  if (len == sizeof(int64_t)) {
    int64_t t64;
    settings_file_get(file, key, key_len, &t64, sizeof(t64));
    *time_out = t64;
    return S_SUCCESS;
  }
  return E_RANGE;
}

status_t settings_file_set_time(SettingsFile *file, const void *key, size_t key_len, time_t time) {
  const int64_t t64 = time;
  return settings_file_set(file, key, key_len, &t64, sizeof(t64));
}

int settings_file_get_len(SettingsFile *file, const void *key, size_t key_len) {
  if (settings_file_exists(file, key, key_len)) {
    const uint8_t key_crc8 = pbl_crc8(0, key, key_len);
    return s_settings_file.val_lens[key_crc8];
  }

  return 0;
}

status_t settings_file_delete(SettingsFile *file, const void *key, size_t key_len) {
  if (settings_file_exists(file, key, key_len)) {
    const uint8_t key_crc8 = pbl_crc8(0, key, key_len);
    free(s_settings_file.values[key_crc8]);
    free(s_settings_file.keys[key_crc8]);
    s_settings_file.values[key_crc8] = nullptr;
    s_settings_file.keys[key_crc8] = nullptr;
    s_settings_file.val_lens[key_crc8] = 0;
    s_settings_file.key_lens[key_crc8] = 0;
    s_settings_file.dirty[key_crc8] = false;
    return S_SUCCESS;
  } else {
    return E_DOES_NOT_EXIST;
  }
}

status_t settings_file_open(SettingsFile *file, const char *name, int max_used_space) {
  if (s_settings_file.open) {
    return E_BUSY;
  } else {
    *file = (SettingsFile){};
    s_settings_file.open = true;
    return S_SUCCESS;
  }
}

status_t settings_file_open_growable(SettingsFile *file, const char *name, int max_used_space,
                                     int initial_alloc_size) {
  return settings_file_open(file, name, max_used_space);
}

status_t settings_file_compact(SettingsFile *file) {
  cl_assert(s_settings_file.open);
  return S_SUCCESS;
}

void settings_file_close(SettingsFile *file) {
  cl_assert(s_settings_file.open);
  s_settings_file.open = false;
}

bool settings_file_exists(SettingsFile *file, const void *key, size_t key_len) {
  const uint8_t key_crc8 = pbl_crc8(0, key, key_len);
  return (s_settings_file.values[key_crc8] != nullptr &&
          memcmp(s_settings_file.keys[key_crc8], key, key_len) == 0);
}

status_t settings_file_mark_synced(SettingsFile *file, const void *key, size_t key_len) {
  if (settings_file_exists(file, key, key_len)) {
    const uint8_t key_crc8 = pbl_crc8(0, key, key_len);
    s_settings_file.dirty[key_crc8] = false;
    return S_SUCCESS;
  }
  return E_DOES_NOT_EXIST;
}

status_t settings_file_set_byte(SettingsFile *file, const void *key, size_t key_len, size_t offset,
                                uint8_t byte) {
  if (settings_file_exists(file, key, key_len)) {
    const uint8_t key_crc8 = pbl_crc8(0, key, key_len);
    uint8_t *val_bytes = s_settings_file.values[key_crc8];
    val_bytes[offset] &= byte;
    return S_SUCCESS;
  } else {
    return E_DOES_NOT_EXIST;
  }
}

uint8_t s_cur_itr;

static void prv_get_key(SettingsFile *file, void *key, size_t key_len) {
  memcpy(key, s_settings_file.keys[s_cur_itr], key_len);
}

static void prv_get_val(SettingsFile *file, void *val, size_t val_len) {
  memcpy(val, s_settings_file.values[s_cur_itr], val_len);
}

status_t settings_file_each(SettingsFile *file, SettingsFileEachCallback cb, void *context) {
  for (unsigned i = 0; i < UINT8_MAX; ++i) {
    if (s_settings_file.keys[i] != nullptr && s_settings_file.values[i] != nullptr) {
      s_cur_itr = i;

      SettingsRecordInfo info = {
        .get_key = prv_get_key,
        .get_val = prv_get_val,
        .key_len = s_settings_file.key_lens[i],
        .val_len = s_settings_file.val_lens[i],
        .dirty = s_settings_file.dirty[i],
      };
      if (!cb(file, &info, context)) {
        break;
      }
    }
  }
  return S_SUCCESS;
}

status_t settings_file_rewrite(SettingsFile *file, SettingsFileRewriteCallback cb, void *context) {
  // TODO
  fake_settings_file_reset();
  return S_SUCCESS;
}

status_t settings_file_rewrite_filtered(SettingsFile *file,
                                        SettingsFileRewriteFilterCallback filter_cb,
                                        void *context) {
  for (unsigned i = 0; i < UINT8_MAX; ++i) {
    if (s_settings_file.keys[i] == nullptr || s_settings_file.values[i] == nullptr) {
      continue;
    }
    if (!filter_cb || filter_cb(s_settings_file.keys[i], s_settings_file.key_lens[i],
                                s_settings_file.values[i], s_settings_file.val_lens[i], context)) {
      continue;
    }
    free(s_settings_file.values[i]);
    s_settings_file.values[i] = nullptr;
    free(s_settings_file.keys[i]);
    s_settings_file.keys[i] = nullptr;
    s_settings_file.val_lens[i] = 0;
    s_settings_file.key_lens[i] = 0;
    s_settings_file.dirty[i] = false;
  }
  return S_SUCCESS;
}
