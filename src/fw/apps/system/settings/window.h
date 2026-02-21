/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "menu.h"

#include "applib/ui/window.h"

Window *settings_window_create(SettingsMenuItem category, SettingsCallbacks *callbacks);

void settings_window_destroy(Window *window);
