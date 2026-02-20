/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/kino/kino_reel.h"

//! Get the size of the provided reel that implements how a launcher app glance should be drawn.
//! @param reel The reel that implements how a glance should be drawn
//! @return The size of the reel
GSize launcher_app_glance_get_size_for_reel(KinoReel *reel);
