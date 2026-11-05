/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/settings/settings.h>

PBL_WEAK int pbl_settings_open(struct pbl_settings_file *file, const char *name,
                               int max_used_space) {
  return 0;
}

PBL_WEAK int pbl_settings_open_growable(struct pbl_settings_file *file, const char *name,
                                        int max_used_space, int initial_alloc_size) {
  return pbl_settings_open(file, name, max_used_space);
}

PBL_WEAK int pbl_settings_get(struct pbl_settings_file *file, const void *key, size_t key_len,
                              void *val_out, size_t val_out_len) {
  return 0;
}

PBL_WEAK int pbl_settings_set(struct pbl_settings_file *file, const void *key, size_t key_len,
                              const void *val, size_t val_len) {
  return 0;
}

PBL_WEAK void pbl_settings_each(struct pbl_settings_file *file, pbl_settings_each_cb_t cb,
                                void *context) {
}

PBL_WEAK size_t pbl_settings_get_len(struct pbl_settings_file *file, const void *key,
                                     size_t key_len) {
  return 0;
}

PBL_WEAK int pbl_settings_delete(struct pbl_settings_file *file, const void *key, size_t key_len) {
  return 0;
}

PBL_WEAK int pbl_settings_compact(struct pbl_settings_file *file) {
  return 0;
}

PBL_WEAK void pbl_settings_close(struct pbl_settings_file *file) {
}

PBL_WEAK void pbl_settings_read_key(struct pbl_settings_file *file, void *buf, size_t buf_len) {
}

PBL_WEAK void pbl_settings_read_val(struct pbl_settings_file *file, void *buf, size_t buf_len) {
}
