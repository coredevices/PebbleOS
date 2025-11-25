/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! Enter manufacturing mode, but does not start the manufacturing app.
void mfg_enter_mfg_mode(void);

//! Enter manufacturing mode and also add a launcher task callback to start the MFG Menu App.
void mfg_enter_mfg_mode_and_launch_app(void);

bool mfg_is_mfg_mode(void);

