/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/crc/crc.h>
#include <pbl/settings/settings.h>

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

void fake_settings_reset(void) {
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

int pbl_settings_get(struct pbl_settings_file *file, const void *key, size_t key_len, void *val_out,
                     size_t val_out_len) {
  if (pbl_settings_exists(file, key, key_len)) {
    const uint8_t key_crc8 = pbl_crc8(0, key, key_len);
    memcpy(val_out, s_settings_file.values[key_crc8], val_out_len);
    return 0;
  }

  memset(val_out, 0, val_out_len);
  return -ENOENT;
}

int pbl_settings_set(struct pbl_settings_file *file, const void *key, size_t key_len,
                     const void *val, size_t val_len) {
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
  return 0;
}

size_t pbl_settings_get_len(struct pbl_settings_file *file, const void *key, size_t key_len) {
  if (pbl_settings_exists(file, key, key_len)) {
    const uint8_t key_crc8 = pbl_crc8(0, key, key_len);
    return s_settings_file.val_lens[key_crc8];
  }

  return 0;
}

int pbl_settings_delete(struct pbl_settings_file *file, const void *key, size_t key_len) {
  if (pbl_settings_exists(file, key, key_len)) {
    const uint8_t key_crc8 = pbl_crc8(0, key, key_len);
    free(s_settings_file.values[key_crc8]);
    free(s_settings_file.keys[key_crc8]);
    s_settings_file.values[key_crc8] = nullptr;
    s_settings_file.keys[key_crc8] = nullptr;
    s_settings_file.val_lens[key_crc8] = 0;
    s_settings_file.key_lens[key_crc8] = 0;
    s_settings_file.dirty[key_crc8] = false;
    return 0;
  } else {
    return -ENOENT;
  }
}

int pbl_settings_open(struct pbl_settings_file *file, const char *name, int max_used_space) {
  if (s_settings_file.open) {
    return -EBUSY;
  } else {
    *file = (struct pbl_settings_file){};
    s_settings_file.open = true;
    return 0;
  }
}

int pbl_settings_open_growable(struct pbl_settings_file *file, const char *name, int max_used_space,
                               int initial_alloc_size) {
  return pbl_settings_open(file, name, max_used_space);
}

int pbl_settings_compact(struct pbl_settings_file *file) {
  cl_assert(s_settings_file.open);
  return 0;
}

void pbl_settings_close(struct pbl_settings_file *file) {
  cl_assert(s_settings_file.open);
  s_settings_file.open = false;
}

bool pbl_settings_exists(struct pbl_settings_file *file, const void *key, size_t key_len) {
  const uint8_t key_crc8 = pbl_crc8(0, key, key_len);
  return (s_settings_file.values[key_crc8] != nullptr &&
          memcmp(s_settings_file.keys[key_crc8], key, key_len) == 0);
}

int pbl_settings_mark_synced(struct pbl_settings_file *file, const void *key, size_t key_len) {
  if (pbl_settings_exists(file, key, key_len)) {
    const uint8_t key_crc8 = pbl_crc8(0, key, key_len);
    s_settings_file.dirty[key_crc8] = false;
    return 0;
  }
  return -ENOENT;
}

int pbl_settings_set_byte(struct pbl_settings_file *file, const void *key, size_t key_len,
                          size_t offset, uint8_t byte) {
  if (pbl_settings_exists(file, key, key_len)) {
    const uint8_t key_crc8 = pbl_crc8(0, key, key_len);
    uint8_t *val_bytes = s_settings_file.values[key_crc8];
    val_bytes[offset] &= byte;
    return 0;
  } else {
    return -ENOENT;
  }
}

uint8_t s_cur_itr;

void pbl_settings_read_key(struct pbl_settings_file *file, void *key, size_t key_len) {
  memcpy(key, s_settings_file.keys[s_cur_itr], key_len);
}

void pbl_settings_read_val(struct pbl_settings_file *file, void *val, size_t val_len) {
  memcpy(val, s_settings_file.values[s_cur_itr], val_len);
}

void pbl_settings_each(struct pbl_settings_file *file, pbl_settings_each_cb_t cb, void *context) {
  for (unsigned i = 0; i < UINT8_MAX; ++i) {
    if (s_settings_file.keys[i] != nullptr && s_settings_file.values[i] != nullptr) {
      s_cur_itr = i;

      struct pbl_settings_record info = {
        .key_len = s_settings_file.key_lens[i],
        .val_len = s_settings_file.val_lens[i],
        .dirty = s_settings_file.dirty[i],
      };
      if (!cb(file, &info, context)) {
        break;
      }
    }
  }
}

int pbl_settings_rewrite(struct pbl_settings_file *file, pbl_settings_rewrite_cb_t cb,
                         void *context) {
  // TODO
  fake_settings_reset();
  return 0;
}

int pbl_settings_rewrite_filtered(struct pbl_settings_file *file,
                                  pbl_settings_filter_cb_t filter_cb, void *context) {
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
  return 0;
}
