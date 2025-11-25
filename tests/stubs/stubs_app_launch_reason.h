/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_launch_reason.h"
#include "util/attributes.h"

uint32_t WEAK app_launch_get_args(void) {
  return 0;
}
