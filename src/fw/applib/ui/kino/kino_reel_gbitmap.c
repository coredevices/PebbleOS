/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kino_reel_gbitmap.h"
#include "kino_reel_gbitmap_private.h"

#include "applib/graphics/gtypes.h"
#include "applib/applib_malloc.auto.h"
#include "syscall/syscall.h"
#include "util/struct.h"

static void prv_destructor(KinoReel *reel) {
  KinoReelImplGBitmap *bitmap_reel = (KinoReelImplGBitmap *)reel;
  if (bitmap_reel->owns_bitmap) {
    gbitmap_destroy(bitmap_reel->bitmap);
  }

  applib_free(bitmap_reel);
}

static void prv_draw_processed_func(KinoReel *reel, GContext *ctx, GPoint offset,
                                    KinoReelProcessor *processor) {
  KinoReelImplGBitmap *bitmap_reel = (KinoReelImplGBitmap *)reel;
  GRect bounds = gbitmap_get_bounds(bitmap_reel->bitmap);
  bounds.origin = gpoint_add(bounds.origin, offset);
  // Save compositing mode
  GCompOp prev_compositing_mode = ctx->draw_state.compositing_mode;

  GCompOp op = (bitmap_reel->bitmap->info.format == GBitmapFormat1Bit)
               ? GCompOpAssign
               : GCompOpSet;

  graphics_context_set_compositing_mode(ctx, op);

  graphics_draw_bitmap_in_rect_processed(ctx, bitmap_reel->bitmap, &bounds,
                                         NULL_SAFE_FIELD_ACCESS(processor, bitmap_processor, NULL));

  // Restore previous compositing mode
  graphics_context_set_compositing_mode(ctx, prev_compositing_mode);
}

static GSize prv_get_size(KinoReel *reel) {
  KinoReelImplGBitmap *bitmap_reel = (KinoReelImplGBitmap *)reel;
  GRect bounds = gbitmap_get_bounds(bitmap_reel->bitmap);
  return bounds.size;
}

static size_t prv_get_data_size(const KinoReel *reel) {
  KinoReelImplGBitmap *bitmap_reel = (KinoReelImplGBitmap *)reel;
  GBitmap *bitmap = bitmap_reel->bitmap;
  size_t palette_size = 0;

  switch (bitmap->info.format) {
    case GBitmapFormat1BitPalette:
      palette_size = 2;
      break;
    case GBitmapFormat2BitPalette:
      palette_size = 4;
      break;
    case GBitmapFormat4BitPalette:
      palette_size = 16;
      break;
    case GBitmapFormat1Bit:
    case GBitmapFormat8Bit:
    case GBitmapFormat8BitCircular:
      break;
  }

  return (bitmap->row_size_bytes * bitmap->bounds.size.h) + palette_size;
}

static GBitmap *prv_get_gbitmap(KinoReel *reel) {
  if (reel) {
    return ((KinoReelImplGBitmap*)reel)->bitmap;
  }
  return NULL;
}

static const KinoReelImpl KINO_REEL_IMPL_GBITMAP = {
  .reel_type = KinoReelTypeGBitmap,
  .destructor = prv_destructor,
  .get_size = prv_get_size,
  .get_data_size = prv_get_data_size,
  .draw_processed = prv_draw_processed_func,
  .get_gbitmap = prv_get_gbitmap,
};

void kino_reel_gbitmap_init(KinoReelImplGBitmap *bitmap_reel, GBitmap *bitmap) {
  if (bitmap_reel) {
    *bitmap_reel = (KinoReelImplGBitmap) {
      .bitmap = bitmap,
      .base.impl = &KINO_REEL_IMPL_GBITMAP
    };
  }
}

KinoReel *kino_reel_gbitmap_create(GBitmap *bitmap, bool take_ownership) {
  KinoReelImplGBitmap *reel = applib_zalloc(sizeof(KinoReelImplGBitmap));
  if (reel) {
    kino_reel_gbitmap_init(reel, bitmap);
    reel->owns_bitmap = take_ownership;
  }

  return (KinoReel *)reel;
}

KinoReel *kino_reel_gbitmap_create_with_resource(uint32_t resource_id) {
  ResAppNum app_num = sys_get_current_resource_num();
  return kino_reel_gbitmap_create_with_resource_system(app_num, resource_id);
}

KinoReel *kino_reel_gbitmap_create_with_resource_system(ResAppNum app_num, uint32_t resource_id) {
  GBitmap *bitmap = gbitmap_create_with_resource_system(app_num, resource_id);
  if (bitmap == NULL) {
    return NULL;
  }
  return kino_reel_gbitmap_create(bitmap, true);
}
