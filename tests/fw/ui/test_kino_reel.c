/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/kino/kino_reel.h"
#include "applib/ui/kino/kino_reel_custom.h"
#include "applib/ui/kino/kino_reel_gbitmap.h"
#include "applib/ui/kino/kino_reel_gbitmap_sequence.h"
#include "applib/ui/kino/kino_reel_pdci.h"
#include "applib/ui/kino/kino_reel_pdcs.h"
#include "util/graphics.h"

#include "clar.h"

// Fakes
////////////////////////////////////
#include "fake_resource_syscalls.h"

// Stubs
////////////////////////////////////
#include "stubs_app_state.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_gpath.h"
#include "stubs_graphics.h"
#include "stubs_graphics_context.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_resources.h"
#include "stubs_ui_window.h"
#include "stubs_unobstructed_area.h"

void framebuffer_clear(FrameBuffer* f) {}
void graphics_context_move_draw_box(GContext* ctx, GPoint offset) {}
typedef uint16_t ResourceId;
const uint8_t *resource_get_builtin_bytes(ResAppNum app_num, uint32_t resource_id,
                                          uint32_t *num_bytes_out) { return NULL; }

// Helper Functions
////////////////////////////////////
#include "../graphics/test_graphics.h"
#include "../graphics/8bit/test_framebuffer.h"

static FrameBuffer *fb = NULL;

// Setup
void test_kino_reel__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  fb->size = (GSize) {DISP_COLS, DISP_ROWS};
}

// Teardown
void test_kino_reel__cleanup(void) {
  free(fb);
}

// Tests
////////////////////////////////////

void test_kino_reel__resource_gbitmap(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Test loading GBitmap Kino Reel
  uint32_t resource_id = sys_resource_load_file_as_resource(
      TEST_IMAGES_PATH, "test_kino_reel__resource_gbitmap.pbi");
  cl_assert(resource_id != UINT32_MAX);

  KinoReel *kino_reel = kino_reel_create_with_resource(resource_id);
  cl_assert(kino_reel);
  cl_assert(kino_reel_get_type(kino_reel) == KinoReelTypeGBitmap);
  cl_assert_equal_i(kino_reel_get_data_size(kino_reel), 2308);

  kino_reel_draw(kino_reel, &ctx, GPointZero);
}

void test_kino_reel__resource_gbitmap_sequence(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // Test loading GBitmap Sequence Kino Reel
  uint32_t resource_id = sys_resource_load_file_as_resource(
      TEST_IMAGES_PATH, "test_kino_reel__resource_gbitmap_sequence.apng");
  cl_assert(resource_id != UINT32_MAX);

  KinoReel *kino_reel = kino_reel_create_with_resource(resource_id);
  cl_assert(kino_reel);
  cl_assert(kino_reel_get_type(kino_reel) == KinoReelTypeGBitmapSequence);
  // We expect the default 0 because get_data_size hasn't been implemented for GBitmapSequence
  cl_assert_equal_i(kino_reel_get_data_size(kino_reel), 0);

  kino_reel_draw(kino_reel, &ctx, GPointZero);
}

void test_kino_reel__resource_pdci(void) {
  // Test loading PDCI Kino Reel
  uint32_t resource_id = sys_resource_load_file_as_resource(
      TEST_IMAGES_PATH, "test_kino_reel__resource_pdci.pdc");
  cl_assert(resource_id != UINT32_MAX);

  KinoReel *kino_reel = kino_reel_create_with_resource(resource_id);
  cl_assert(kino_reel);
  cl_assert(kino_reel_get_type(kino_reel) == KinoReelTypePDCI);
  cl_assert_equal_i(kino_reel_get_data_size(kino_reel), 192);
}

void test_kino_reel__resource_pdcs(void) {
  // Test loading PDCS Kino Reel
  uint32_t resource_id = sys_resource_load_file_as_resource(
      TEST_IMAGES_PATH, "test_kino_reel__resource_pdcs.pdc");
  cl_assert(resource_id != UINT32_MAX);

  KinoReel *kino_reel = kino_reel_create_with_resource(resource_id);
  cl_assert(kino_reel);
  cl_assert(kino_reel_get_type(kino_reel) == KinoReelTypePDCS);
  cl_assert_equal_i(kino_reel_get_data_size(kino_reel), 356);
}

void test_kino_reel__verify_pdci_get_list(void) {
  // Test loading PDCI Kino Reel
  uint32_t resource_id = sys_resource_load_file_as_resource(
      TEST_IMAGES_PATH, "test_kino_reel__resource_pdci.pdc");
  cl_assert(resource_id != UINT32_MAX);

  KinoReel *kino_reel = kino_reel_create_with_resource(resource_id);
  cl_assert(kino_reel);
  cl_assert(kino_reel_get_type(kino_reel) == KinoReelTypePDCI);

  // Verify that list retrieved from kino_reel same as from command_image
  GDrawCommandList *list = kino_reel_get_gdraw_command_list(kino_reel);
  GDrawCommandList *list_direct = gdraw_command_image_get_command_list(
      kino_reel_get_gdraw_command_image(kino_reel));
  cl_assert(list == list_direct && list != NULL);
}

void test_kino_reel__verify_pdcs_get_list(void) {
  // Test loading PDCI Kino Reel
  uint32_t resource_id = sys_resource_load_file_as_resource(
      TEST_IMAGES_PATH, "test_kino_reel__resource_pdcs.pdc");
  cl_assert(resource_id != UINT32_MAX);

  KinoReel *kino_reel = kino_reel_create_with_resource(resource_id);
  cl_assert(kino_reel);
  cl_assert(kino_reel_get_type(kino_reel) == KinoReelTypePDCS);

  kino_reel_set_elapsed(kino_reel, 0);
  GDrawCommandList *list1 = kino_reel_get_gdraw_command_list(kino_reel);
  GDrawCommandList *list1_direct = gdraw_command_frame_get_command_list(
      gdraw_command_sequence_get_frame_by_elapsed(
        kino_reel_get_gdraw_command_sequence(kino_reel), 0));
  cl_assert(list1 != NULL);
  cl_assert(list1 == list1_direct);

  // Test that after elapsed, frame has changed and new list is correct
  kino_reel_set_elapsed(kino_reel, 100);
  GDrawCommandList *list2 = kino_reel_get_gdraw_command_list(kino_reel);
  GDrawCommandList *list2_direct = gdraw_command_frame_get_command_list(
      gdraw_command_sequence_get_frame_by_elapsed(
        kino_reel_get_gdraw_command_sequence(kino_reel), 100));
  cl_assert(list2 != NULL);
  cl_assert(list2 != list1);
  cl_assert(list2 == list2_direct);
}

static KinoReelProcessor s_dummy_processor;

static void prv_dummy_impl_draw_processed(KinoReel *reel, GContext *ctx, GPoint offset,
                                          KinoReelProcessor *processor) {
  cl_assert_equal_p(processor, &s_dummy_processor);
}

void test_kino_reel__draw_processed(void) {
  // Calling kino_reel_draw_processed() should pass the processor to the .draw_processed function
  const KinoReelImpl dummy_impl = (KinoReelImpl) {
    .draw_processed = prv_dummy_impl_draw_processed,
  };
  KinoReel *kino_reel = kino_reel_custom_create(&dummy_impl, NULL);
  GContext ctx;
  kino_reel_draw_processed(kino_reel, &ctx, GPointZero, &s_dummy_processor);
}

