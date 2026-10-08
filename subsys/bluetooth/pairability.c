/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pairability_priv.h"

#include <pbl/bluetooth/pairability.h>

static bool s_pairable;

void pbl_bt_le_pairability_set_enabled(bool enabled) {
  s_pairable = enabled;
}

bool pairability_is_enabled(void) {
  return s_pairable;
}
