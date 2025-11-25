/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "shell/shell.h"

#include "kernel/events.h"
#include "process_management/app_install_types.h"

const CompositorTransition* shell_get_close_compositor_animation(AppInstallId current_app_id,
                                                                 AppInstallId next_app_id) {
  return NULL;
}

const CompositorTransition* shell_get_open_compositor_animation(AppInstallId current_app_id,
                                                                AppInstallId next_app_id) {
  return NULL;
}
