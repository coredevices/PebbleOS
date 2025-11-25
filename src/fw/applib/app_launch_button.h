/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/button_id.h"

//! @internal
//! Get the button id used to launch the app.
//! Only valid if the launch reason is APP_LAUNCH_USER or APP_LAUNCH_QUICK_LAUNCH.
ButtonId app_launch_button(void);
