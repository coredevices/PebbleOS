/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kino_reel_pdci.h"

#include "applib/applib_malloc.auto.h"
#include "syscall/syscall.h"
#include "util/struct.h"

typedef struct {
  KinoReel base;
  GDrawCommandImage *image;
  bool owns_image;
} KinoReelImplPDCI;

static void prv_destructor(KinoReel *reel) {
  KinoReelImplPDCI *dci_reel = (KinoReelImplPDCI *)reel;
  if (dci_reel->owns_image) {
    gdraw_command_image_destroy(dci_reel->image);
  }

  applib_free(dci_reel);
}

static void prv_draw_processed_func(KinoReel *reel, GContext *ctx, GPoint offset,
                                    KinoReelProcessor *processor) {
  KinoReelImplPDCI *dci_reel = (KinoReelImplPDCI *)reel;

  gdraw_command_image_draw_processed(
    ctx, dci_reel->image, offset, NULL_SAFE_FIELD_ACCESS(processor, draw_command_processor, NULL));
}

static GSize prv_get_size(KinoReel *reel) {
  KinoReelImplPDCI *dci_reel = (KinoReelImplPDCI *)reel;
  return gdraw_command_image_get_bounds_size(dci_reel->image);
}

static size_t prv_get_data_size(const KinoReel *reel) {
  KinoReelImplPDCI *dci_reel = (KinoReelImplPDCI *)reel;
  return gdraw_command_image_get_data_size(dci_reel->image);
}

static GDrawCommandImage *prv_get_gdraw_command_image(KinoReel *reel) {
  if (reel) {
    return ((KinoReelImplPDCI*)reel)->image;
  }
  return NULL;
}

static GDrawCommandList *prv_get_gdraw_command_list(KinoReel *reel) {
  if (reel) {
    return gdraw_command_image_get_command_list(((KinoReelImplPDCI*)reel)->image);
  }
  return NULL;
}

static const KinoReelImpl KINO_REEL_IMPL_PDCI = {
  .reel_type = KinoReelTypePDCI,
  .destructor = prv_destructor,
  .get_size = prv_get_size,
  .get_data_size = prv_get_data_size,
  .draw_processed = prv_draw_processed_func,
  .get_gdraw_command_image = prv_get_gdraw_command_image,
  .get_gdraw_command_list = prv_get_gdraw_command_list,
};

KinoReel *kino_reel_pdci_create(GDrawCommandImage *image, bool take_ownership) {
  KinoReelImplPDCI *reel = applib_zalloc(sizeof(KinoReelImplPDCI));
  if (reel) {
    reel->image = image;
    reel->owns_image = take_ownership;
    reel->base.impl = &KINO_REEL_IMPL_PDCI;
  }

  return (KinoReel *)reel;
}

KinoReel *kino_reel_pdci_create_with_resource(uint32_t resource_id) {
  ResAppNum app_num = sys_get_current_resource_num();
  return kino_reel_pdci_create_with_resource_system(app_num, resource_id);
}

KinoReel *kino_reel_pdci_create_with_resource_system(ResAppNum app_num, uint32_t resource_id) {
  GDrawCommandImage *image = gdraw_command_image_create_with_resource_system(app_num, resource_id);
  if (image == NULL) {
    return NULL;
  }
  return kino_reel_pdci_create(image, true);
}
