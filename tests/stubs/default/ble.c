/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/kernel/compiler.h>

PBL_WEAK bool gaps_init(void) {
  return true;
}

PBL_WEAK bool gaps_deinit(uint32_t stack_id) {
  return true;
}

PBL_WEAK void comm_handle_paired_devices_changed(void) {
}
