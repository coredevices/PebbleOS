/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
