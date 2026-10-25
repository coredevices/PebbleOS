/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/button_id.h>
#include <pbl/util/uuid.h>

#include <applib/ui/recognizer/swipe.h>
#include <process_management/app_install_types.h>

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

bool quick_launch_combo_up_down_is_enabled(void);
AppInstallId quick_launch_combo_up_down_get_app(void);
void quick_launch_combo_up_down_set_app(AppInstallId app_id);
void quick_launch_combo_up_down_set_enabled(bool enabled);

// Accepts one cardinal direction, not a direction mask.
bool quick_launch_swipe_is_enabled(SwipeDirection direction);
AppInstallId quick_launch_swipe_get_app(SwipeDirection direction);
void quick_launch_swipe_set_app(SwipeDirection direction, AppInstallId app_id);
void quick_launch_swipe_set_enabled(SwipeDirection direction, bool enabled);
