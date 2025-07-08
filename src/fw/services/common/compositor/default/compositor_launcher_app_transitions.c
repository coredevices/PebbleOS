/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "compositor_launcher_app_transitions.h"

#include "applib/graphics/bitblt.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics_private.h"
#include "apps/system_apps/launcher/default/launcher_app.h"
#include "services/common/compositor/compositor_transitions.h"
#include "system/passert.h"
#include "util/attributes.h"
#include "util/math.h"
#include "util/size.h"

typedef struct CompositorLauncherAppTransitionData {
  bool app_is_destination;
  LauncherDrawState launcher_draw_state;
  int16_t prev_delta_x_before_cut;
} CompositorLauncherAppTransitionData;

static CompositorLauncherAppTransitionData s_data;

// This custom moook curve was created with iterative feedback from the Design team
static const int32_t s_custom_moook_frames_in[] = {0, 1, 2, 4, 12, 24, 48};
static const int32_t s_custom_moook_frames_out[] = {12, 6, 3, 2, 1, 0};
static const MoookConfig s_custom_moook_config = {
  .frames_in = s_custom_moook_frames_in,
  .num_frames_in = ARRAY_LENGTH(s_custom_moook_frames_in),
  .frames_out = s_custom_moook_frames_out,
  .num_frames_out = ARRAY_LENGTH(s_custom_moook_frames_out),
};

static void prv_move_region_of_bitmap_horizontally(GBitmap *bitmap, const GRect *region,
                                                   int16_t delta_x) {
  if (!bitmap || !region) {
    return;
  }
  GBitmap region_sub_bitmap;
  gbitmap_init_as_sub_bitmap(&region_sub_bitmap, bitmap, *region);
  graphics_private_move_pixels_horizontally(&region_sub_bitmap, delta_x, true /* patch_garbage */);
}

static void prv_copy_app_fb_patching_garbage(int16_t dest_origin_x) {
  const GBitmap src_bitmap = compositor_get_app_framebuffer_as_bitmap();
  GBitmap dest_bitmap = compositor_get_framebuffer_as_bitmap();

  // Patch garbage pixels using the first/last column, if necessary
  if (dest_origin_x != 0) {
    const bool offscreen_left = (dest_origin_x < 0);
    const int16_t first_column_x = 0;
    const int16_t last_column_x = DISP_COLS - 1;
    const GRect column_to_replicate = (GRect) {
      .origin = GPoint((offscreen_left ? last_column_x : first_column_x), 0),
      .size = GSize(1, DISP_ROWS),
    };
    GBitmap column_to_replicate_sub_bitmap;
    gbitmap_init_as_sub_bitmap(&column_to_replicate_sub_bitmap, &src_bitmap, column_to_replicate);
    const int16_t from = (int16_t)(offscreen_left ? (dest_origin_x + DISP_COLS) : first_column_x);
    const int16_t to = (int16_t)(offscreen_left ? last_column_x : dest_origin_x - 1);
    for (int16_t x = from; x <= to; x++) {
      bitblt_bitmap_into_bitmap(&dest_bitmap, &column_to_replicate_sub_bitmap,
                                GPoint(x, 0), GCompOpAssign, GColorWhite);
    }
  }

  bitblt_bitmap_into_bitmap(&dest_bitmap, &src_bitmap, GPoint(dest_origin_x, 0), GCompOpAssign,
                            GColorWhite);
}

static void prv_manipulate_launcher_in_system_framebuffer(GContext *ctx,
                                                          const GRect *selection_rect,
                                                          int16_t delta, GColor selection_color) {
  if (!ctx || !selection_rect || (delta == 0)) {
    return;
  }

  // Move the selection rectangle
  prv_move_region_of_bitmap_horizontally(&ctx->dest_bitmap, selection_rect, delta);

  const int16_t abs_delta = (int16_t)ABS(delta);

  // Move everything above the selection rectangle (if there is anything) and stretch the selection
  // color up
  const int16_t area_above_selection_rect_height = selection_rect->origin.y;
  if (area_above_selection_rect_height > 0) {
    const GRect area_above_selection_rect = GRect(-selection_rect->origin.x, 0, DISP_COLS,
                                                  area_above_selection_rect_height);
    prv_move_region_of_bitmap_horizontally(&ctx->dest_bitmap, &area_above_selection_rect, -delta);

    const GRect stretch_rect_above_selection_rect =
        GRect(0, selection_rect->origin.y - abs_delta, DISP_COLS, abs_delta);
    graphics_context_set_fill_color(ctx, selection_color);
    graphics_fill_rect(ctx, &stretch_rect_above_selection_rect);
  }

  // Move everything below the selection rectangle (if there is anything) and stretch the selection
  // color down
  const int16_t row_below_selection_rect_bottom = grect_get_max_y(selection_rect);
  const int16_t area_below_selection_rect_height =
      (int16_t)DISP_ROWS - row_below_selection_rect_bottom;
  if (area_below_selection_rect_height > 0) {
    const GRect area_below_selection_rect = GRect(-selection_rect->origin.x,
                                                  row_below_selection_rect_bottom, DISP_COLS,
                                                  area_below_selection_rect_height);
    prv_move_region_of_bitmap_horizontally(&ctx->dest_bitmap, &area_below_selection_rect, -delta);

    const GRect stretch_rect_below_selection_rect = GRect(0, row_below_selection_rect_bottom,
                                                          DISP_COLS, abs_delta);
    graphics_context_set_fill_color(ctx, selection_color);
    graphics_fill_rect(ctx, &stretch_rect_below_selection_rect);
  }
}

static void prv_launcher_app_transition_animation_update(GContext *ctx, Animation *PBL_UNUSED animation,
                                                         uint32_t distance_normalized) {
  const bool is_right = s_data.app_is_destination;
  const GRangeVertical selection_vertical_range =
      s_data.launcher_draw_state.selection_vertical_range;
  const GColor selection_color = s_data.launcher_draw_state.selection_background_color;

  const int16_t start = 0;
  const int16_t end = DISP_COLS;
  const int16_t delta_x_before_cut = interpolate_int16(distance_normalized, start, end);
  const int16_t delta_x_before_cut_diff = delta_x_before_cut - s_data.prev_delta_x_before_cut;
  const int16_t delta_x_after_cut = interpolate_int16(distance_normalized, -end, start);

  // This rect specifies where the launcher's selected row currently is in the system framebuffer
  const GRect selection_rect = GRect((is_right ? s_data.prev_delta_x_before_cut : start),
                                     selection_vertical_range.origin_y, DISP_COLS,
                                     selection_vertical_range.size_h);

  // We know we're before the moook cut if our delta for after the cut hasn't "moooked" beyond
  // where we will finish the animation
  const bool before_cut = (delta_x_after_cut < start);
  if (before_cut) {
    if (is_right) {
      // Manipulate the launcher's pixels in the system framebuffer so the selection moves from its
      // starting point right and everything else moves left
      prv_manipulate_launcher_in_system_framebuffer(ctx, &selection_rect, delta_x_before_cut_diff,
                                                    selection_color);
    } else {
      // Move the system framebuffer's pixels from its starting point right
      const bool patch_garbage = true;
      graphics_private_move_pixels_horizontally(&ctx->dest_bitmap, delta_x_before_cut_diff,
                                                patch_garbage);
    }

    // Save the delta we used so we can calculate the diff for the next frame
    s_data.prev_delta_x_before_cut = delta_x_before_cut;
  } else {
    const int16_t dest_origin_x = (is_right ? -delta_x_after_cut : start);
    // Copy the entire app framebuffer (containing the launcher) to the compositor framebuffer
    prv_copy_app_fb_patching_garbage(dest_origin_x);
    if (!is_right) {
      // Manipulate the launcher's pixels in the system framebuffer so the selection moves right
      // and everything else moves left so everything comes to rest at its final position
      prv_manipulate_launcher_in_system_framebuffer(ctx, &selection_rect, -delta_x_after_cut,
                                                    selection_color);
    }
  }

  // Technically the whole framebuffer may not be dirty after each frame (and thus not need to be
  // marked as such so we don't flush every scan line to the display), but let's make it easy and
  // just dirty the whole framebuffer on each frame anyway since most pixels do change
  framebuffer_dirty_all(compositor_get_framebuffer());
}

static int64_t prv_launcher_app_transition_custom_moook(int32_t progress, int64_t from,
                                                        int64_t to) {
  return interpolate_moook_custom(progress, from, to, &s_custom_moook_config);
}

static uint32_t prv_launcher_app_transition_custom_moook_duration(void) {
  return interpolate_moook_custom_duration(&s_custom_moook_config);
}

static void prv_launcher_app_transition_animation_init(Animation *animation) {
  // Grab the draw state now that the launcher has had a chance to save its state before closing
  const LauncherDrawState *launcher_draw_state = launcher_app_get_draw_state();
  PBL_ASSERTN(launcher_draw_state);
  s_data.launcher_draw_state = *launcher_draw_state;

  animation_set_custom_interpolation(animation, prv_launcher_app_transition_custom_moook);
  animation_set_duration(animation, prv_launcher_app_transition_custom_moook_duration());
}

const CompositorTransition *compositor_launcher_app_transition_get(bool app_is_destination) {
  if (compositor_transition_app_to_app_should_be_skipped()) {
    return NULL;
  }

  s_data = (CompositorLauncherAppTransitionData) {
    .app_is_destination = app_is_destination,
  };

  static const CompositorTransition s_impl = {
    .init = prv_launcher_app_transition_animation_init,
    .update = prv_launcher_app_transition_animation_update,
  };
  return &s_impl;
}
