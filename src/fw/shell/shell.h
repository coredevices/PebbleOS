/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_install_types.h"

typedef struct CompositorTransition CompositorTransition;

const CompositorTransition *shell_get_watchface_compositor_animation(bool watchface_is_destination);

const CompositorTransition *shell_get_close_compositor_animation(AppInstallId current_app_id,
                                                                 AppInstallId next_app_id);

const CompositorTransition *shell_get_open_compositor_animation(AppInstallId current_app_id,
                                                                AppInstallId next_app_id);
