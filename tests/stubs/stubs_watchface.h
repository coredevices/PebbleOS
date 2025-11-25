/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "shell/normal/watchface.h"
#include "util/attributes.h"

AppInstallId WEAK watchface_get_default_install_id(void) {
  return 0;
}

void WEAK watchface_set_default_install_id(const AppInstallId app_id) {}

void WEAK watchface_launch_default(const CompositorTransition *animation) {}
