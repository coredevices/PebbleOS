/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/crc/crc.h>
#include <pbl/kernel/time.h>
#include <pbl/settings/settings.h>

#include <clar_asserts.h>

struct {
  bool open;
  void *values[UINT8_MAX];
  void *keys[UINT8_MAX];
  uint32_t val_lens[UINT8_MAX];
  uint32_t key_lens[UINT8_MAX];
  bool dirty[UINT8_MAX];
  uint32_t last_modified[UINT8_MAX];
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
    s_settings_file.last_modified[i] = 0;
  }
  s_settings_file.open = false;
}

//! Slot holding the key, or -1. Keys are placed by CRC, moving on when slots collide.
static int prv_find(const void *key, size_t key_len) {
  for (unsigned i = 0; i < UINT8_MAX; ++i) {
    if (s_settings_file.keys[i] != nullptr && s_settings_file.key_lens[i] == key_len &&
        memcmp(s_settings_file.keys[i], key, key_len) == 0) {
      return i;
    }
  }
  return -1;
}

//! Slot holding the key, or a free one to put it in
static int prv_find_or_alloc(const void *key, size_t key_len) {
  const int found = prv_find(key, key_len);
  if (found >= 0) {
    return found;
  }
  const unsigned start = pbl_crc8(0, key, key_len) % UINT8_MAX;
  for (unsigned n = 0; n < UINT8_MAX; ++n) {
    const unsigned i = (start + n) % UINT8_MAX;
    if (s_settings_file.keys[i] == nullptr) {
      return i;
    }
  }
  cl_fail("fake settings file full");
  return -1;
}

int pbl_settings_get(struct pbl_settings_file *file, const void *key, size_t key_len, void *val_out,
                     size_t val_out_len) {
  const int i = prv_find(key, key_len);
  if (i >= 0) {
    if (val_out_len > s_settings_file.val_lens[i]) {
      memset(val_out, 0, val_out_len);
      return -ERANGE;
    }
    memcpy(val_out, s_settings_file.values[i], val_out_len);
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
  const int key_crc8 = prv_find_or_alloc(key, key_len);

  free(s_settings_file.values[key_crc8]);
  free(s_settings_file.keys[key_crc8]);
  s_settings_file.values[key_crc8] = val_copy;
  s_settings_file.keys[key_crc8] = key_copy;
  s_settings_file.val_lens[key_crc8] = val_len;
  s_settings_file.key_lens[key_crc8] = key_len;
  s_settings_file.dirty[key_crc8] = true;
  s_settings_file.last_modified[key_crc8] = (uint32_t)pbl_realtime();
  return 0;
}

int pbl_settings_get_time(struct pbl_settings_file *file, const void *key, size_t key_len,
                          time_t *time_out) {
  *time_out = 0;
  const size_t len = pbl_settings_get_len(file, key, key_len);
  if (len == 0) {
    return -ENOENT;
  }
  if (len == sizeof(uint32_t)) {
    uint32_t t32;
    pbl_settings_get(file, key, key_len, &t32, sizeof(t32));
    *time_out = t32;
    return 0;
  }
  if (len == sizeof(int64_t)) {
    int64_t t64;
    pbl_settings_get(file, key, key_len, &t64, sizeof(t64));
    *time_out = t64;
    return 0;
  }
  return -ERANGE;
}

int pbl_settings_set_time(struct pbl_settings_file *file, const void *key, size_t key_len,
                          time_t time) {
  const int64_t t64 = time;
  return pbl_settings_set(file, key, key_len, &t64, sizeof(t64));
}

size_t pbl_settings_get_len(struct pbl_settings_file *file, const void *key, size_t key_len) {
  const int i = prv_find(key, key_len);
  if (i >= 0) {
    return s_settings_file.val_lens[i];
  }

  return 0;
}

int pbl_settings_delete(struct pbl_settings_file *file, const void *key, size_t key_len) {
  const int key_crc8 = prv_find(key, key_len);
  if (key_crc8 >= 0) {
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
  return prv_find(key, key_len) >= 0;
}

int pbl_settings_mark_synced(struct pbl_settings_file *file, const void *key, size_t key_len) {
  if (pbl_settings_exists(file, key, key_len)) {
    s_settings_file.dirty[prv_find(key, key_len)] = false;
    return 0;
  }
  return -ENOENT;
}

int pbl_settings_set_byte(struct pbl_settings_file *file, const void *key, size_t key_len,
                          size_t offset, uint8_t byte) {
  if (pbl_settings_exists(file, key, key_len)) {
    uint8_t *val_bytes = s_settings_file.values[prv_find(key, key_len)];
    val_bytes[offset] &= byte;
    return 0;
  } else {
    return -ENOENT;
  }
}

uint8_t s_cur_itr;

// The records a rewrite reads from, while its callback writes to the (emptied) store
static struct {
  bool active;
  void *values[UINT8_MAX];
  void *keys[UINT8_MAX];
  uint32_t val_lens[UINT8_MAX];
  uint32_t key_lens[UINT8_MAX];
  bool dirty[UINT8_MAX];
  uint32_t last_modified[UINT8_MAX];
} s_rewrite_old;

void pbl_settings_read_key(struct pbl_settings_file *file, void *key, size_t key_len) {
  void *const *keys = s_rewrite_old.active ? s_rewrite_old.keys : s_settings_file.keys;
  memcpy(key, keys[s_cur_itr], key_len);
}

void pbl_settings_read_val(struct pbl_settings_file *file, void *val, size_t val_len) {
  void *const *values = s_rewrite_old.active ? s_rewrite_old.values : s_settings_file.values;
  memcpy(val, values[s_cur_itr], val_len);
}

void pbl_settings_each(struct pbl_settings_file *file, pbl_settings_each_cb_t cb, void *context) {
  for (unsigned i = 0; i < UINT8_MAX; ++i) {
    if (s_settings_file.keys[i] != nullptr && s_settings_file.values[i] != nullptr) {
      s_cur_itr = i;

      struct pbl_settings_record info = {
        .key_len = s_settings_file.key_lens[i],
        .val_len = s_settings_file.val_lens[i],
        .dirty = s_settings_file.dirty[i],
        .last_modified = s_settings_file.last_modified[i],
      };
      if (!cb(file, &info, context)) {
        break;
      }
    }
  }
}

int pbl_settings_rewrite(struct pbl_settings_file *file, pbl_settings_rewrite_cb_t cb,
                         void *context) {
  // Move the records aside, then let the callback write back what it keeps
  for (unsigned i = 0; i < UINT8_MAX; ++i) {
    s_rewrite_old.values[i] = s_settings_file.values[i];
    s_rewrite_old.keys[i] = s_settings_file.keys[i];
    s_rewrite_old.val_lens[i] = s_settings_file.val_lens[i];
    s_rewrite_old.key_lens[i] = s_settings_file.key_lens[i];
    s_rewrite_old.dirty[i] = s_settings_file.dirty[i];
    s_rewrite_old.last_modified[i] = s_settings_file.last_modified[i];
    s_settings_file.values[i] = nullptr;
    s_settings_file.keys[i] = nullptr;
    s_settings_file.val_lens[i] = 0;
    s_settings_file.key_lens[i] = 0;
    s_settings_file.dirty[i] = false;
    s_settings_file.last_modified[i] = 0;
  }
  s_rewrite_old.active = true;
  for (unsigned i = 0; i < UINT8_MAX; ++i) {
    if (s_rewrite_old.keys[i] == nullptr || s_rewrite_old.values[i] == nullptr) {
      continue;
    }
    s_cur_itr = i;
    struct pbl_settings_record info = {
      .key_len = s_rewrite_old.key_lens[i],
      .val_len = s_rewrite_old.val_lens[i],
      .dirty = s_rewrite_old.dirty[i],
      .last_modified = s_rewrite_old.last_modified[i],
    };
    cb(file, file, &info, context);
  }
  s_rewrite_old.active = false;
  for (unsigned i = 0; i < UINT8_MAX; ++i) {
    free(s_rewrite_old.values[i]);
    free(s_rewrite_old.keys[i]);
    s_rewrite_old.values[i] = nullptr;
    s_rewrite_old.keys[i] = nullptr;
  }
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
