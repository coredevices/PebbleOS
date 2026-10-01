/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @file app_custom_icon.h
//! This file is part of app_install_manager.c
//! It provides a Pebble protocol endpoint to allow 3rd party apps
//! to customize the title and icon of certain stock apps, like the "Sports" app.

#include "pebble_process_md.h"
#include "applib/graphics/gtypes.h"
#include "process_management/app_install_types.h"

const char *app_custom_get_title(AppInstallId app_id);

const GBitmap *app_custom_get_icon(AppInstallId app_id);
