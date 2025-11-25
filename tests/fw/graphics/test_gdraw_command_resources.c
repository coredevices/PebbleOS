/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/graphics/gdraw_command.h"
#include "applib/graphics/gdraw_command_list.h"
#include "applib/graphics/gdraw_command_image.h"
#include "applib/graphics/gdraw_command_private.h"

#include "applib/graphics/gtypes.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_line.h"
#include "applib/graphics/gpath.h"

#include <string.h>

#include "util.h"  // graphics tests utils

#include "fake_pbl_malloc.h"
#include "fake_resource_syscalls.h"

#include "stubs_app_state.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_graphics_context.h"
#include "stubs_gpath.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_passert.h"

// Stubs
void graphics_draw_circle(GContext* ctx, GPoint p, uint16_t radius) {}
void graphics_fill_circle(GContext* ctx, GPoint p, uint16_t radius) {}
void framebuffer_clear(FrameBuffer* f){}
void graphics_context_move_draw_box(GContext* ctx, GPoint offset) {}
void graphics_line_draw_precise_stroked(GContext* ctx, GPointPrecise p0, GPointPrecise p1) {}
const uint8_t *resource_get_builtin_bytes(ResAppNum app_num, uint32_t resource_id,
					  uint32_t *num_bytes_out) { return NULL; }

void test_gdraw_command_resources__load_pdci(void) {
  uint32_t resource_id = sys_resource_load_file_as_resource(TEST_IMAGES_PATH, TEST_PDC_FILE);
  cl_assert(resource_id != UINT32_MAX);
  GDrawCommandImage *image = gdraw_command_image_create_with_resource(resource_id);
  cl_assert(image != NULL);
}

void test_gdraw_command_resources__load_pdcs(void) {
  uint32_t resource_id = sys_resource_load_file_as_resource(TEST_IMAGES_PATH, TEST_PDC_FILE);
  cl_assert(resource_id != UINT32_MAX);
  GDrawCommandSequence *sequence = gdraw_command_sequence_create_with_resource(resource_id);
  cl_assert(sequence != NULL);
}

// Test that loading an invalid PDC file fails
void test_gdraw_command_resources__load_invalid(void) {
  uint32_t resource_id = sys_resource_load_file_as_resource(TEST_IMAGES_PATH, TEST_PDC_FILE);
  cl_assert(resource_id != UINT32_MAX);
  
  // Test Command_Image
  GDrawCommandImage *image = gdraw_command_image_create_with_resource(resource_id);
  cl_assert(image == NULL);
  
  // Test Command Sequence
  GDrawCommandSequence *sequence = gdraw_command_sequence_create_with_resource(resource_id);
  cl_assert(sequence == NULL);
}

