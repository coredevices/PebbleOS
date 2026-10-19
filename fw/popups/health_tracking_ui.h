/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! Show a modal with a message and text with an optional action bar
void health_tracking_ui_show_message(uint32_t res_id, const char *text, bool show_action_bar);

//! Show the modal that tells the user that health tracking is disabled
//! and a given feature will not work as expected
void health_tracking_ui_feature_show_disabled(void);
