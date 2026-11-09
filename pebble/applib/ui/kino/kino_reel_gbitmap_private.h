/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef struct {
  KinoReel base;
  GBitmap *bitmap;
  bool owns_bitmap;
} KinoReelImplGBitmap;

void kino_reel_gbitmap_init(KinoReelImplGBitmap *bitmap_reel, GBitmap *bitmap);
