/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/language_pack.h>

void PBL_WEAK language_pack_set_resource(uint32_t resource_id) {
}

void PBL_WEAK language_pack_enable(bool enable) {
}

bool PBL_WEAK language_pack_get_info(uint8_t slot, struct pbl_i18n_catalog_info *info) {
  return false;
}

bool PBL_WEAK language_pack_has_fonts(uint8_t slot) {
  return false;
}

bool PBL_WEAK language_pack_remove(uint8_t slot) {
  return false;
}

const char *PBL_WEAK language_pack_install_begin(const char *filename) {
  return filename;
}

void PBL_WEAK language_pack_install_end(bool success) {
}
