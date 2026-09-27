/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_custom_icon.h"
#include "pbl/kernel/compiler.h"

const char *PBL_WEAK app_custom_get_title(AppInstallId app_id) {
  return "";
}
