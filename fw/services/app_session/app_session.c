/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/app_session.h>

#include <process_management/app_manager.h>

struct pbl_pp_session *app_session_get_current(void) {
  const PebbleProcessMd *md = app_manager_get_current_app_md();

  if (md == nullptr) {
    return nullptr;
  }
  if (md->allow_js) {
    return pbl_pp_session_get_system();
  }

  return pbl_pp_session_get_app(&md->uuid);
}

void app_session_sanitize(struct pbl_pp_session **session) {
  struct pbl_pp_session *permitted = app_session_get_current();

  if (*session == nullptr || *session == permitted) {
    *session = permitted;
  } else {
    *session = nullptr;
  }
}
