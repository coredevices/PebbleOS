/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/settings/settings.h>

PBL_WEAK status_t pbl_settings_open(struct pbl_settings_file *file, const char *name,
                                    int max_used_space) {
  return S_SUCCESS;
}

PBL_WEAK status_t pbl_settings_open_growable(struct pbl_settings_file *file, const char *name,
                                             int max_used_space, int initial_alloc_size) {
  return pbl_settings_open(file, name, max_used_space);
}

PBL_WEAK status_t pbl_settings_get(struct pbl_settings_file *file, const void *key, size_t key_len,
                                   void *val_out, size_t val_out_len) {
  return S_SUCCESS;
}

PBL_WEAK status_t pbl_settings_set(struct pbl_settings_file *file, const void *key, size_t key_len,
                                   const void *val, size_t val_len) {
  return S_SUCCESS;
}

PBL_WEAK void pbl_settings_each(struct pbl_settings_file *file, pbl_settings_each_cb_t cb,
                                void *context) {
}

PBL_WEAK int pbl_settings_get_len(struct pbl_settings_file *file, const void *key, size_t key_len) {
  return 0;
}

PBL_WEAK status_t pbl_settings_delete(struct pbl_settings_file *file, const void *key,
                                      size_t key_len) {
  return S_SUCCESS;
}

PBL_WEAK status_t pbl_settings_compact(struct pbl_settings_file *file) {
  return S_SUCCESS;
}

PBL_WEAK void pbl_settings_close(struct pbl_settings_file *file) {
}
