/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kino_reel.h"

KinoReel *kino_reel_gbitmap_create(GBitmap *bitmap, bool take_ownership);

KinoReel *kino_reel_gbitmap_create_with_resource(uint32_t resource_id);

KinoReel *kino_reel_gbitmap_create_with_resource_system(ResAppNum app_num, uint32_t resource_id);
