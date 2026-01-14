/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/button_id.h"
#include "process_management/app_install_types.h"
#include "util/uuid.h"

bool quick_launch_is_enabled(ButtonId button);
AppInstallId quick_launch_get_app(ButtonId button);
void quick_launch_set_app(ButtonId button, AppInstallId app_id);
void quick_launch_set_enabled(ButtonId button, bool enabled);
void quick_launch_set_quick_launch_setup_opened(uint8_t version);
uint8_t quick_launch_get_quick_launch_setup_opened(void);

bool quick_launch_single_click_is_enabled(ButtonId button);
AppInstallId quick_launch_single_click_get_app(ButtonId button);
void quick_launch_single_click_set_app(ButtonId button, AppInstallId app_id);
void quick_launch_single_click_set_enabled(ButtonId button, bool enabled);

bool quick_launch_combo_back_up_is_enabled(void);
AppInstallId quick_launch_combo_back_up_get_app(void);
void quick_launch_combo_back_up_set_app(AppInstallId app_id);
void quick_launch_combo_back_up_set_enabled(bool enabled);