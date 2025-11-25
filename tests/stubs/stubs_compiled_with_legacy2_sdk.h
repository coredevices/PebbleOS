/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

static bool s_is_legacy2 = false;

void process_manager_set_compiled_with_legacy2_sdk(bool is_legacy2) {
  s_is_legacy2 = is_legacy2;
}

bool process_manager_compiled_with_legacy2_sdk(void) {
#if LEGACY2_TEST
  return true;
#else
  return s_is_legacy2;
#endif
}

