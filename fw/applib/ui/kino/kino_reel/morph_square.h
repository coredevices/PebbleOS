/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/kino/kino_reel.h"

//! A KinoReel that can transform an image to a square or an image to another
//! with a square as an intermediate.
//! @param from_reel KinoReel to begin with
//! @param take_ownership true if this KinoReel will free `image` when destroyed.
//! @return a morph to square KinoReel
//! @see gpoint_attract_to_square
KinoReel *kino_reel_morph_square_create(KinoReel *from_reel, bool take_ownership);
