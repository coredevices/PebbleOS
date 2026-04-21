/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "menu.h"

#include "applib/ui/window.h"

Window *settings_window_create(SettingsMenuItem category, SettingsCallbacks *callbacks);

//! Create a settings window that reuses a parent category's identity (for
//! mark-dirty / reload routing) but shows a distinct title. Use this for
//! nested submenus under an existing top-level category — e.g. a "Backlight"
//! submenu pushed from within the "Display" settings.
//! @param category The owning top-level SettingsMenuItem for dirty routing.
//! @param title    Status-bar title for this window. Must outlive the window.
//! @param callbacks Row-handling callbacks for this submenu.
Window *settings_window_create_with_title(SettingsMenuItem category, const char *title,
                                          SettingsCallbacks *callbacks);

void settings_window_destroy(Window *window);
