/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kino_reel_custom.h"

#include "applib/applib_malloc.auto.h"

const uint32_t CUSTOM_REEL_CANARY = 0xbaebaef8;

typedef struct {
  KinoReel base;
  uint32_t canary;
  const KinoReelImpl *impl;
  void *data;
} KinoReelImplCustom;

static void prv_destructor(KinoReel *reel) {
  KinoReelImplCustom *custom_reel = (KinoReelImplCustom *)reel;
  if (custom_reel->impl->destructor) {
    custom_reel->impl->destructor(reel);
  }

  applib_free(custom_reel);
}

static uint32_t prv_elapsed_getter(KinoReel *reel) {
  KinoReelImplCustom *custom_reel = (KinoReelImplCustom *)reel;
  if (custom_reel->impl->get_elapsed) {
    return custom_reel->impl->get_elapsed(reel);
  }

  return 0;
}

static bool prv_elapsed_setter(KinoReel *reel, uint32_t elapsed_ms) {
  KinoReelImplCustom *custom_reel = (KinoReelImplCustom *)reel;
  if (custom_reel->impl->set_elapsed) {
    return custom_reel->impl->set_elapsed(reel, elapsed_ms);
  }

  return false;
}

static uint32_t prv_duration_getter(KinoReel *reel) {
  KinoReelImplCustom *custom_reel = (KinoReelImplCustom *)reel;
  if (custom_reel->impl->get_duration) {
    return custom_reel->impl->get_duration(reel);
  }

  return 0;
}

static GSize prv_size_getter(KinoReel *reel) {
  KinoReelImplCustom *custom_reel = (KinoReelImplCustom *)reel;
  if (custom_reel->impl->get_size) {
    return custom_reel->impl->get_size(reel);
  }

  return GSize(0, 0);
}

static void prv_draw_processed_func(KinoReel *reel, GContext *ctx, GPoint offset,
                                    KinoReelProcessor *processor) {
  KinoReelImplCustom *custom_reel = (KinoReelImplCustom *)reel;
  if (custom_reel->impl->draw_processed) {
    custom_reel->impl->draw_processed(reel, ctx, offset, processor);
  }
}

GDrawCommandImage *prv_get_gdraw_command_image(KinoReel *reel) {
  KinoReelImplCustom *custom_reel = (KinoReelImplCustom *)reel;
  if (custom_reel->impl->get_gdraw_command_image) {
    return custom_reel->impl->get_gdraw_command_image(reel);
  }

  return NULL;
}

GDrawCommandList *prv_get_gdraw_command_list(KinoReel *reel) {
  KinoReelImplCustom *custom_reel = (KinoReelImplCustom *)reel;
  if (custom_reel->impl->get_gdraw_command_list) {
    return custom_reel->impl->get_gdraw_command_list(reel);
  }

  return NULL;
}

GDrawCommandSequence *prv_get_gdraw_command_sequence(KinoReel *reel) {
  KinoReelImplCustom *custom_reel = (KinoReelImplCustom *)reel;
  if (custom_reel->impl->get_gdraw_command_sequence) {
    return custom_reel->impl->get_gdraw_command_sequence(reel);
  }

  return NULL;
}

GBitmap *prv_get_gbitmap(KinoReel *reel) {
  KinoReelImplCustom *custom_reel = (KinoReelImplCustom *)reel;
  if (custom_reel->impl->get_gbitmap) {
    return custom_reel->impl->get_gbitmap(reel);
  }

  return NULL;
}

GBitmapSequence* prv_get_gbitmap_sequence(KinoReel *reel) {
  KinoReelImplCustom *custom_reel = (KinoReelImplCustom *)reel;
  if (custom_reel->impl->get_gbitmap_sequence) {
    return custom_reel->impl->get_gbitmap_sequence(reel);
  }

  return NULL;
}

static const KinoReelImpl KINO_REEL_IMPL_CUSTOM = {
  .reel_type = KinoReelTypeCustom,
  .destructor = prv_destructor,
  .get_elapsed = prv_elapsed_getter,
  .set_elapsed = prv_elapsed_setter,
  .get_duration = prv_duration_getter,
  .get_size = prv_size_getter,
  .draw_processed = prv_draw_processed_func,
  .get_gdraw_command_image = prv_get_gdraw_command_image,
  .get_gdraw_command_list = prv_get_gdraw_command_list,
  .get_gdraw_command_sequence = prv_get_gdraw_command_sequence,
  .get_gbitmap = prv_get_gbitmap,
  .get_gbitmap_sequence = prv_get_gbitmap_sequence,
};

KinoReel *kino_reel_custom_create(const KinoReelImpl *custom_impl, void *data) {
  KinoReelImplCustom *reel = applib_zalloc(sizeof(KinoReelImplCustom));
  if (reel) {
    reel->base.impl = &KINO_REEL_IMPL_CUSTOM;
    reel->canary = CUSTOM_REEL_CANARY;
    reel->impl = custom_impl;
    reel->data = data;
  }

  return (KinoReel *)reel;
}

static bool prv_kino_reel_custom_is_custom(KinoReel *reel) {
  KinoReelImplCustom *custom_reel = (KinoReelImplCustom *)reel;
  return (custom_reel->canary == CUSTOM_REEL_CANARY);
}

void *kino_reel_custom_get_data(KinoReel *reel) {
  if (!prv_kino_reel_custom_is_custom(reel)) {
    return NULL;
  }
  KinoReelImplCustom *custom_reel = (KinoReelImplCustom *)reel;
  return custom_reel->data;
}
