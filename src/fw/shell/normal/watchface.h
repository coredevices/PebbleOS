/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"
#include "process_management/app_install_manager.h"
#include "services/common/compositor/compositor.h"

void watchface_init(void);

void watchface_handle_button_event(PebbleEvent *e);

void watchface_set_default_install_id(AppInstallId id);

AppInstallId watchface_get_default_install_id(void);

void watchface_launch_default(const CompositorTransition *animation);

void watchface_start_low_power(void);

void watchface_reset_click_manager(void);
