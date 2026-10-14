/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/ui/kino/kino_reel.h>

PBL_WEAK KinoReel *kino_reel_create_with_resource(uint32_t resource_id) {
  return nullptr;
}

PBL_WEAK KinoReel *kino_reel_create_with_resource_system(ResAppNum app_num, uint32_t resource_id) {
  return nullptr;
}

PBL_WEAK void kino_reel_destroy(KinoReel *reel) {
}

PBL_WEAK void kino_reel_draw_processed(KinoReel *reel, GContext *ctx, GPoint offset,
                                       KinoReelProcessor *processor) {
}

PBL_WEAK void kino_reel_draw(KinoReel *reel, GContext *ctx, GPoint offset) {
}

PBL_WEAK bool kino_reel_set_elapsed(KinoReel *reel, uint32_t elapsed_ms) {
  return false;
}

PBL_WEAK uint32_t kino_reel_get_elapsed(KinoReel *reel) {
  return 0;
}

PBL_WEAK uint32_t kino_reel_get_duration(KinoReel *reel) {
  return 0;
}

PBL_WEAK GSize kino_reel_get_size(KinoReel *reel) {
  return (GSize){};
}

PBL_WEAK GDrawCommandImage *kino_reel_get_gdraw_command_image(KinoReel *reel) {
  return nullptr;
}

PBL_WEAK GDrawCommandList *kino_reel_get_gdraw_command_list(KinoReel *reel) {
  return nullptr;
}

PBL_WEAK GDrawCommandSequence *kino_reel_get_gdraw_command_sequence(KinoReel *reel) {
  return nullptr;
}

PBL_WEAK GBitmap *kino_reel_get_gbitmap(KinoReel *reel) {
  return nullptr;
}

PBL_WEAK GBitmapSequence *kino_reel_get_gbitmap_sequence(KinoReel *reel) {
  return nullptr;
}

PBL_WEAK KinoReelType kino_reel_get_type(KinoReel *reel) {
  return 0;
}

PBL_WEAK KinoReel *kino_reel_custom_create(const KinoReelImpl *custom_impl, void *data) {
  return nullptr;
}

PBL_WEAK void *kino_reel_custom_get_data(KinoReel *reel) {
  return nullptr;
}

PBL_WEAK size_t kino_reel_get_data_size(const KinoReel *reel) {
  return 0;
}
