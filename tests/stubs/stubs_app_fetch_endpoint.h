/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/app_fetch_endpoint.h"
#include "util/attributes.h"

bool WEAK app_fetch_in_progress(void) {
  return false;
}

void WEAK app_fetch_cancel_from_system_task(AppInstallId app_id) {}

