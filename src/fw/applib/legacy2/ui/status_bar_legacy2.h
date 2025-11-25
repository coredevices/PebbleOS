/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"

//! @file status_bar_legacy2.h
//!
//! This file implements a 2.x status bar for backwards compatibility with apps compiled with old
//! firmwares.

#define STATUS_BAR_HEIGHT 16

#define STATUS_BAR_FRAME GRect(0, 0, DISP_COLS, STATUS_BAR_HEIGHT)
