/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "shell/normal/quick_launch.h"
#include "util/attributes.h"

void WEAK quick_launch_remove_app(const Uuid *uuid) {}

AppInstallId WEAK quick_launch_get_app(ButtonId button) {
  return 0;
}
