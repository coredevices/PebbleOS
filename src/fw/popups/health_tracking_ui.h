/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_install_types.h"

//! Show a modal with a message and text with an optional action bar
void health_tracking_ui_show_message(uint32_t res_id, const char *text, bool show_action_bar);

//! Show the modal that tells the user that health tracking is disabled
//! and a given app will not work as expected
void health_tracking_ui_app_show_disabled(void);

//! Show the modal that tells the user that health tracking is disabled
//! and a given feature will not work as expected
void health_tracking_ui_feature_show_disabled(void);

//! Inform the health tracking UI that a new app got launched
void health_tracking_ui_register_app_launch(AppInstallId app_id);
