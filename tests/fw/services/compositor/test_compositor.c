/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/compositor/compositor.h>
#include <pbl/services/compositor/compositor_private.h>

#include <applib/graphics/framebuffer.h>
#include <applib/graphics/gcontext.h>
#include <applib/graphics/gtypes.h>
#include <clar.h>
#include <fff.h>
#include <kernel/events.h>
#include <kernel/ui/modals/modal_manager.h>

// Stubs
///////////////////////////////////////////////////////////

extern void prv_handle_display_update_complete(bool success);

static int s_count_animation_create = 0;
Animation *animation_create(void) {
  ++s_count_animation_create;

  return (Animation *)(uintptr_t)s_count_animation_create;
}

static int s_count_animation_schedule = 0;
static Animation *s_scheduled_animation;
bool animation_schedule(Animation *animation) {
  ++s_count_animation_schedule;
  s_scheduled_animation = animation;

  return true;
}

bool animation_set_auto_destroy(Animation *animation, bool auto_destroy) {
  return true;
}

bool animation_is_scheduled(Animation *animation_h) {
  return animation_h && animation_h == s_scheduled_animation;
}

static int s_count_animation_destroy = 0;
bool animation_unschedule(Animation *animation) {
  ++s_count_animation_destroy;
  s_scheduled_animation = nullptr;
  return true;
}

bool animation_destroy(Animation *animation) {
  ++s_count_animation_destroy;
  s_scheduled_animation = nullptr;
  return true;
}

static int s_app_window_render_count;
static bool s_app_framebuffer_fetched;
static FrameBuffer s_app_framebuffer;
FrameBuffer *app_state_get_framebuffer(void) {
  // Not a great proxy for app rendering but good enough. The compositor fetches the app
  // framebuffer while rendering the app and nowhere else, so the first fetch of each render
  // (reset below when the render finishes) counts as one render.
  if (!s_app_framebuffer_fetched) {
    s_app_framebuffer_fetched = true;
    ++s_app_window_render_count;
  }

  return &s_app_framebuffer;
}

void app_manager_get_framebuffer_size(GSize *size) {
  *size = (GSize){DISP_COLS, DISP_ROWS};
}

static int s_count_bitblt;
static GRect s_last_bitblt_rect;
void bitblt_bitmap_into_bitmap(GBitmap *dest_bitmap, const GBitmap *src_bitmap, GPoint dest_offset,
                               GCompOp compositing_mode, GColor tint_color) {
  ++s_count_bitblt;
  s_last_bitblt_rect = (GRect){dest_offset, src_bitmap->bounds.size};
  // The compositor must never copy outside the display
  cl_assert(dest_offset.x >= 0 && dest_offset.y >= 0);
  cl_assert(dest_offset.x + src_bitmap->bounds.size.w <= DISP_COLS);
  cl_assert(dest_offset.y + src_bitmap->bounds.size.h <= DISP_ROWS);
  // Copy the pixels too, so the system framebuffer really holds the frame afterwards
  for (int16_t y = 0; y < src_bitmap->bounds.size.h; y++) {
    memcpy((uint8_t *)dest_bitmap->addr + (dest_offset.y + y) * dest_bitmap->row_size_bytes +
               dest_offset.x,
           (const uint8_t *)src_bitmap->addr +
               (src_bitmap->bounds.origin.y + y) * src_bitmap->row_size_bytes +
               src_bitmap->bounds.origin.x,
           src_bitmap->bounds.size.w);
  }
}

GBitmapDataRowInfo gbitmap_get_data_row_info(const GBitmap *bitmap, uint16_t y) {
  // The compositor must never compare rows outside the display
  cl_assert(y < DISP_ROWS);
  return (GBitmapDataRowInfo){
    .data = (uint8_t *)bitmap->addr + y * bitmap->row_size_bytes,
    .min_x = 0,
    .max_x = grect_get_max_x(&bitmap->bounds) - 1,
  };
}

static GRect s_last_marked_rect;
void framebuffer_mark_dirty_rect(FrameBuffer *fb, GRect rect) {
  s_last_marked_rect = rect;
  fb->dirty_rect = fb->is_dirty ? grect_union(&fb->dirty_rect, &rect) : rect;
  fb->is_dirty = true;
}

void framebuffer_reset_dirty(FrameBuffer *fb) {
  fb->dirty_rect = GRectZero;
  fb->is_dirty = false;
}

// Back the framebuffer bitmaps with real buffers sized for the display, one for the app and
// one for the system framebuffer. compositor_render_app() memsets framebuffer_get_size_bytes()
// bytes through the system one's addr, and compares and copies rows between the two.
static uint8_t s_framebuffer_data[DISP_COLS * DISP_ROWS];
static uint8_t s_app_framebuffer_data[DISP_COLS * DISP_ROWS];

// A transparent modal that paints a strip at the bottom of the screen, like the timeline peek
static const GRect s_modal_strip = {{0, 200}, {DISP_COLS, 28}};
static void prv_modal_render_strip(GContext *ctx) {
  memset(s_framebuffer_data + s_modal_strip.origin.y * DISP_COLS, 0x5a,
         s_modal_strip.size.h * DISP_COLS);
  framebuffer_mark_dirty_rect(compositor_get_framebuffer(), s_modal_strip);
}

void compositor_dot_transition_app_to_app_init(Animation *animation) {
}

bool compositor_dot_transition_app_to_app_update_func(GContext *ctx, Animation *animation,
                                                      uint32_t distance_normalized) {
  return true;
}

static bool s_modal_window_present = false;
static bool s_modal_window_transparent = false;
Window *modal_manager_get_top_window(void) {
  return (Window *)(uintptr_t)s_modal_window_present;
}

FAKE_VOID_FUNC(modal_manager_render, GContext *);

ModalProperty modal_manager_get_properties(void) {
  if (!s_modal_window_present) {
    return ModalPropertyDefault;
  }
  return s_modal_window_transparent ? (ModalProperty_Exists | ModalProperty_Transparent)
                                    : ModalProperty_Exists;
}

GContext *kernel_ui_get_graphics_context(void) {
  static GContext s_context;
  return &s_context;
}

void framebuffer_clear(FrameBuffer *f) {
}

void framebuffer_dirty_all(FrameBuffer *fb) {
}

GBitmap framebuffer_get_as_bitmap(FrameBuffer *fb, const GSize *size) {
  return (GBitmap){
    .addr = (fb == &s_app_framebuffer) ? s_app_framebuffer_data : s_framebuffer_data,
    .row_size_bytes = DISP_COLS,
    .bounds = GRect(0, 0, size->w, size->h),
  };
}

void framebuffer_set_line(FrameBuffer *f, uint8_t y, const uint8_t *buffer) {
}

GDrawState graphics_context_get_drawing_state(GContext *ctx) {
  return (GDrawState){};
}

void graphics_context_set_drawing_state(GContext *ctx, GDrawState draw_state) {
}

AnimationPrivate *animation_private_animation_find(Animation *handle) {
  return nullptr;
}

static int s_count_display_update = 0;
static bool s_display_update_success = true;
void compositor_display_update(void (*handle_update_complete_cb)(bool success)) {
  ++s_count_display_update;
  // Like the real one, a flush drops the mirror of the app framebuffer
  compositor_app_framebuffer_mirror_invalidate();
  // The update completes at once, as it does in QEMU
  if (handle_update_complete_cb) {
    handle_update_complete_cb(s_display_update_success);
  }
}

static bool s_display_update_in_progress = false;
bool compositor_display_update_in_progress(void) {
  return s_display_update_in_progress;
}

static PebbleEvent s_last_event;
void event_put(PebbleEvent *event) {
  s_last_event = *event;
}

static bool s_render_pending;
bool process_manager_send_event_to_process(PebbleTask task, PebbleEvent *event) {
  if (event->type == PEBBLE_RENDER_FINISHED_EVENT) {
    s_render_pending = false;
    s_app_framebuffer_fetched = false;
  }
  s_last_event = *event;
  return true;
}

FAKE_VALUE_FUNC(bool, animation_set_implementation, Animation *, const AnimationImplementation *);

static int s_count_compositor_init_func_a = 0;
static void prv_compositor_init_func_a(Animation *animation) {
  ++s_count_compositor_init_func_a;
}

static void prv_compositor_update_func_a(GContext *ctx, Animation *animation,
                                         uint32_t distance_normalized) {
}

static int s_count_compositor_init_func_b = 0;
static void prv_compositor_init_func_b(Animation *animation) {
  ++s_count_compositor_init_func_b;
}

static void prv_compositor_update_func_b(GContext *ctx, Animation *animation,
                                         uint32_t distance_normalized) {
}

static const CompositorTransition s_transition_a = {
  .init = prv_compositor_init_func_a, .update = prv_compositor_update_func_a
};

static const CompositorTransition s_transition_b = {
  .init = prv_compositor_init_func_b, .update = prv_compositor_update_func_b
};

static void (*s_launcher_callback)(void *data);
static void *s_launcher_callback_data;
void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  s_launcher_callback = callback;
  s_launcher_callback_data = data;
}

static void prv_run_launcher_callback(void) {
  void (*callback)(void *data) = s_launcher_callback;
  s_launcher_callback = nullptr;
  callback(s_launcher_callback_data);
}

static int s_count_frozen = 0;
static void *s_frozen_data;
static void prv_frozen_cb(void *data) {
  ++s_count_frozen;
  s_frozen_data = data;
}

// Tests
///////////////////////////////////////////////////////////

void test_compositor__initialize(void) {
  RESET_FAKE(animation_set_implementation);
  animation_set_implementation_fake.return_val = true;

  s_last_event = (PebbleEvent){.type = 0};

  s_modal_window_present = false;
  s_modal_window_transparent = false;

  s_count_animation_create = 0;
  s_count_animation_schedule = 0;
  s_count_animation_destroy = 0;
  s_scheduled_animation = nullptr;

  s_count_display_update = 0;
  s_display_update_success = true;
  s_count_compositor_init_func_a = 0;
  s_count_compositor_init_func_b = 0;

  s_app_window_render_count = 0;
  s_app_framebuffer_fetched = false;
  s_app_framebuffer = (FrameBuffer){.size = {DISP_COLS, DISP_ROWS}};
  memset(s_framebuffer_data, 0, sizeof(s_framebuffer_data));
  memset(s_app_framebuffer_data, 0, sizeof(s_app_framebuffer_data));
  s_count_bitblt = 0;
  s_last_bitblt_rect = GRectZero;
  s_last_marked_rect = GRectZero;
  RESET_FAKE(modal_manager_render);

  s_display_update_in_progress = false;

  s_render_pending = false;

  s_launcher_callback = nullptr;
  s_launcher_callback_data = nullptr;
  s_count_frozen = 0;
  s_frozen_data = nullptr;

  compositor_init();
}

void test_compositor__cleanup(void) {
}

void test_compositor__simple(void) {
  compositor_transition(&s_transition_a);

  // The animation should be created but not scheduled, as we're waiting for the app to render
  cl_assert_equal_i(s_count_animation_create, 1);
  cl_assert_equal_i(s_count_compositor_init_func_a, 1);
  cl_assert_equal_i(s_count_animation_schedule, 0);

  // Make the app render, now the animation should be scheduled
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_animation_schedule, 1);

  compositor_transition(&s_transition_b);

  // We should create a second animation, calling the b's transition init func. The animation
  // should not be scheduled as we're waiting for the interrupted app to render
  cl_assert_equal_i(s_count_animation_create, 2);
  cl_assert_equal_i(s_count_animation_schedule, 1);
  cl_assert_equal_i(s_count_compositor_init_func_a, 1);
  cl_assert_equal_i(s_count_compositor_init_func_b, 1);

  // Make the app render, now the animation should be scheduled
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_animation_schedule, 2);

  // Push a modal window mid transition, the resulting animation should be scheduled immediately.
  s_modal_window_present = true;
  compositor_transition(&s_transition_b);
  cl_assert_equal_i(s_count_animation_create, 3);
  cl_assert_equal_i(s_count_animation_schedule, 3);
  cl_assert_equal_i(s_count_compositor_init_func_a, 1);
  cl_assert_equal_i(s_count_compositor_init_func_b, 2);
}

void test_compositor__app_render_busy(void) {
  // Set the display as busy and then render the app. Nothing should update.
  s_display_update_in_progress = true;
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_display_update, 0);

  // Now fake the display update completeing. The app should know draw.
  s_display_update_in_progress = false;
  prv_handle_display_update_complete(true /* success */);
  cl_assert_equal_i(s_count_display_update, 1);

  // Subsequent app updates should now draw straight through.
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_display_update, 2);

  // test animation updates
  s_display_update_in_progress = true;
  // start a transition
  compositor_transition(&s_transition_a);
  cl_assert_equal_i(s_count_display_update, 2);

  // app render will be handled before transition
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_display_update, 2);
  cl_assert_equal_i(s_count_animation_schedule, 0);

  // The deferred transition now fires. Since this is an App-to-App transition, the compositor
  // requests a fresh app render and schedules the transition animation rather than rendering the
  // app framebuffer straight to the display (see commit 6dcc0ff6a, which added the missing
  // prv_send_app_render_request() to the App-to-App branch to fix a frozen-screen bug). The
  // deferred app render that follows is consumed by scheduling the animation, so no extra display
  // update happens here.
  s_display_update_in_progress = false;
  prv_handle_display_update_complete(true /* success */);
  cl_assert_equal_i(s_count_display_update, 2);
  cl_assert_equal_i(s_count_animation_schedule, 1);

  // A subsequent app render arrives mid-transition; the animation is already scheduled and the
  // display is not updated again here.
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_animation_schedule, 1);
  cl_assert_equal_i(s_count_display_update, 2);
}

void test_compositor__modal_transition_cancels_deferred_app(void) {
  // Set the display as busy and then render the app. Nothing should update.
  s_display_update_in_progress = true;
  s_render_pending = true;
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_display_update, 0);
  cl_assert_equal_i(s_render_pending, true);

  // Now transition to a modal. The app framebuffer should be released. No animation should be
  // started.
  s_modal_window_present = true;
  compositor_transition(&s_transition_a);
  cl_assert_equal_i(s_render_pending, false);
  cl_assert_equal_i(s_count_animation_create, 0);

  // Start the animation
  s_display_update_in_progress = false;
  prv_handle_display_update_complete(true /* success */);
  cl_assert_equal_i(s_count_animation_create, 1);
  cl_assert_equal_i(s_count_animation_schedule, 1);
}

void test_compositor__app_no_animation(void) {
  // Start a transition. We shouldn't update the screen because the app hasn't rendered
  // yet.
  compositor_transition(nullptr);
  cl_assert_equal_i(s_count_display_update, 0);
  cl_assert_equal_i(s_count_animation_create, 0);

  // Now the app has rendered something and we should actually update the display.
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_display_update, 1);
}

// Paints the app framebuffer within rect with a value it has not had before
static void prv_paint_app_framebuffer(GRect rect) {
  static uint8_t s_paint;
  ++s_paint;
  const GRect display_rect = GRect(0, 0, DISP_COLS, DISP_ROWS);
  grect_clip(&rect, &display_rect);
  for (int16_t y = rect.origin.y; y < grect_get_max_y(&rect); y++) {
    memset(s_app_framebuffer_data + y * DISP_COLS + rect.origin.x, s_paint, rect.size.w);
  }
}

// Marks rect dirty in the app framebuffer and paints it, as an app's drawing would
static void prv_mark_app_framebuffer_dirty(GRect rect) {
  s_app_framebuffer.dirty_rect = rect;
  s_app_framebuffer.is_dirty = true;
  prv_paint_app_framebuffer(rect);
}

void test_compositor__app_frame_copies_dirty_rect(void) {
  const GRect full_rect = GRect(0, 0, DISP_COLS, DISP_ROWS);
  const GRect dirty_rect = GRect(10, 20, 30, 40);

  // Nothing mirrors the app framebuffer yet, so the first frame is copied in full and the
  // app's dirty rect is consumed
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_display_update, 1);
  cl_assert_equal_i(s_count_bitblt, 1);
  cl_assert(grect_equal(&s_last_bitblt_rect, &full_rect));
  cl_assert(!s_app_framebuffer.is_dirty);

  // The next frame copies and marks only what the app touched
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_display_update, 2);
  cl_assert_equal_i(s_count_bitblt, 2);
  cl_assert(grect_equal(&s_last_bitblt_rect, &dirty_rect));
  cl_assert(grect_equal(&s_last_marked_rect, &dirty_rect));
  cl_assert(!s_app_framebuffer.is_dirty);

  // A rect from app memory is clipped to the display before it is used
  prv_mark_app_framebuffer_dirty(GRect(-10, DISP_ROWS - 5, 50, 100));
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_bitblt, 3);
  const GRect clipped_rect = GRect(0, DISP_ROWS - 5, 40, 5);
  cl_assert(grect_equal(&s_last_bitblt_rect, &clipped_rect));
  cl_assert(grect_equal(&s_last_marked_rect, &clipped_rect));

  // A frame with nothing dirty copies nothing
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_bitblt, 3);
}

void test_compositor__app_frame_copies_changed_rows_only(void) {
  const GRect full_rect = GRect(0, 0, DISP_COLS, DISP_ROWS);

  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &full_rect));

  // An app that marks the whole screen but only changed a few rows gets those rows copied
  s_app_framebuffer.dirty_rect = full_rect;
  s_app_framebuffer.is_dirty = true;
  prv_paint_app_framebuffer(GRect(50, 100, 20, 11));
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_bitblt, 2);
  const GRect changed_rows = GRect(0, 100, DISP_COLS, 11);
  cl_assert(grect_equal(&s_last_bitblt_rect, &changed_rows));
  cl_assert(grect_equal(&s_last_marked_rect, &changed_rows));

  // Marking the whole screen without changing a pixel copies nothing
  s_app_framebuffer.dirty_rect = full_rect;
  s_app_framebuffer.is_dirty = true;
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_bitblt, 2);
}

void test_compositor__app_frame_ignores_alpha_bits(void) {
  const GRect full_rect = GRect(0, 0, DISP_COLS, DISP_ROWS);

  // An opaque frame, every pixel with its alpha bits set
  memset(s_app_framebuffer_data, 0xff, sizeof(s_app_framebuffer_data));
  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &full_rect));

  // The display driver converted the flushed rows in place and cleared their alpha bits, as the
  // SF32LB one does, which must not count as a change
  for (size_t i = 0; i < sizeof(s_framebuffer_data); i++) {
    s_framebuffer_data[i] &= 0x3f;
  }
  s_app_framebuffer.dirty_rect = full_rect;
  s_app_framebuffer.is_dirty = true;
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_bitblt, 1);

  // A real change in the colour bits still is one
  s_app_framebuffer.dirty_rect = full_rect;
  s_app_framebuffer.is_dirty = true;
  prv_paint_app_framebuffer(GRect(0, 30, DISP_COLS, 2));
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_bitblt, 2);
  const GRect changed_rows = GRect(0, 30, DISP_COLS, 2);
  cl_assert(grect_equal(&s_last_bitblt_rect, &changed_rows));
}

void test_compositor__app_frame_full_after_modal(void) {
  const GRect full_rect = GRect(0, 0, DISP_COLS, DISP_ROWS);
  const GRect dirty_rect = GRect(10, 20, 30, 40);

  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &full_rect));

  // A modal is pushed and popped without animation; its frames flush over the app's
  s_modal_window_present = true;
  compositor_transition(nullptr);
  cl_assert_equal_i(modal_manager_render_fake.call_count, 1);
  s_modal_window_present = false;
  compositor_transition(nullptr);

  // The app's first frame after the modal is composited in full again, the one after it is not
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &full_rect));
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &dirty_rect));
}

void test_compositor__app_frame_under_transparent_modal(void) {
  const GRect full_rect = GRect(0, 0, DISP_COLS, DISP_ROWS);
  const GRect dirty_rect = GRect(10, 20, 30, 40);

  // A transparent modal sits over the app, drawing a strip at the bottom
  s_modal_window_present = true;
  s_modal_window_transparent = true;
  modal_manager_render_fake.custom_fake = prv_modal_render_strip;
  compositor_transition(nullptr);
  compositor_app_render_ready();
  cl_assert_equal_i(modal_manager_render_fake.call_count, 1);
  cl_assert(grect_equal(&s_last_bitblt_rect, &full_rect));

  // The next frame copies the app's dirty rect and the strip the modal drew, then draws the
  // modal again; the system framebuffer's dirty rect covers both
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert_equal_i(modal_manager_render_fake.call_count, 2);
  const GRect expected_copy = grect_union(&dirty_rect, &s_modal_strip);
  cl_assert(grect_equal(&s_last_bitblt_rect, &expected_copy));
  cl_assert(grect_equal(&compositor_get_framebuffer()->dirty_rect, &expected_copy));

  // Once the modal draws nothing (hidden offscreen), only the app's rect is copied, after one
  // more frame that restores the app's pixels where the modal was
  modal_manager_render_fake.custom_fake = nullptr;
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &expected_copy));
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &dirty_rect));
}

void test_compositor__app_frame_full_after_transition(void) {
  const GRect full_rect = GRect(0, 0, DISP_COLS, DISP_ROWS);
  const GRect dirty_rect = GRect(10, 20, 30, 40);

  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &full_rect));

  // A new app without a transition animation: its first frame is composited in full even
  // though it only marked a corner dirty
  compositor_transition(nullptr);
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &full_rect));
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &dirty_rect));
}

void test_compositor__app_frame_rejects_wrapped_rect(void) {
  // A transparent modal strip sits over the app, like the timeline peek
  s_modal_window_present = true;
  s_modal_window_transparent = true;
  modal_manager_render_fake.custom_fake = prv_modal_render_strip;
  compositor_transition(nullptr);
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_bitblt, 1);

  // A rect from app memory whose clipping wraps around in 16 bits is dropped rather than copied
  // from outside the display, so only the modal's strip is copied back
  s_app_framebuffer.dirty_rect = GRect(0, INT16_MIN, 10, INT16_MAX);
  s_app_framebuffer.is_dirty = true;
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_bitblt, 2);
  cl_assert(grect_equal(&s_last_bitblt_rect, &s_modal_strip));
}

void test_compositor__app_frame_full_after_mirror_invalidate(void) {
  const GRect full_rect = GRect(0, 0, DISP_COLS, DISP_ROWS);
  const GRect dirty_rect = GRect(10, 20, 30, 40);

  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &full_rect));

  // The system framebuffer or the panel changed behind the compositor's back, as on a display
  // rotation, so the next frame is composited in full again
  compositor_app_framebuffer_mirror_invalidate();
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &full_rect));
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &dirty_rect));
}

void test_compositor__app_frame_full_after_failed_flush(void) {
  const GRect full_rect = GRect(0, 0, DISP_COLS, DISP_ROWS);
  const GRect dirty_rect = GRect(10, 20, 30, 40);

  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &full_rect));

  // A partial frame's rows never reach the panel, so the frame after it is sent in full
  s_display_update_success = false;
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &dirty_rect));
  s_display_update_success = true;
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &full_rect));
  prv_mark_app_framebuffer_dirty(dirty_rect);
  compositor_app_render_ready();
  cl_assert(grect_equal(&s_last_bitblt_rect, &dirty_rect));
}

void test_compositor__app_not_ready_modal_push_pop(void) {
  // If a modal window is popped revealing an app that has not yet rendered itself for the first
  // time we shouldn't render the app immediately. We need to still wait for the app to render
  // itself for the first time. This is a known issue that doesn't seem to happen too frequently
  // in practice, but we should fix it up at some point.

  // Start a null window transition to an app. It should wait for the app to report ready
  compositor_transition(nullptr);
  cl_assert_equal_i(s_count_display_update, 0);
  cl_assert_equal_i(s_count_animation_create, 0);

  // Push a modal window with an animation and then pop it without an animation. The app still
  // shouldn't be ready.
  s_modal_window_present = true;
  compositor_transition(&s_transition_a);
  cl_assert_equal_i(s_count_animation_create, 1);

  s_modal_window_present = false;
  compositor_transition(nullptr);

  // previous animation is unscheduled
  cl_assert_equal_i(s_count_animation_destroy, 1);

  // Throughout pushing this modal the app never reported it was ready, so we still shouldn't have
  // rendered from the app frame buffer.
  cl_assert_equal_i(s_app_window_render_count, 0);

  // Now the app has rendered something and we should actually update the display.
  compositor_app_render_ready();
  cl_assert_equal_i(s_app_window_render_count, 1);
}

void test_compositor__app_not_ready_cancelled_animation_deferred(void) {
  // Show a modal window
  s_modal_window_present = true;
  compositor_transition(nullptr);

  // Now pop it with a transition
  s_modal_window_present = false;
  compositor_transition(&s_transition_a);
  cl_assert_equal_i(s_count_animation_create, 1);

  // Pretend we're in the middle of copying a frame to the display hardware
  s_display_update_in_progress = true;

  // Start a null window transition to an app while the modal is popping. It should wait for the
  // app to report ready. This shouldn't cancel any animation, as the animation isn't started yet,
  // as it should be waiting for the app to render for the first time before starting.
  compositor_transition(nullptr);
  cl_assert_equal_i(s_count_animation_destroy, 1);
  cl_assert_equal_i(s_app_window_render_count, 0);

  // Now complete the animation teardown by finishing the copy to the display. We shouldn't render
  // the app because it hasn't rendered anything yet.
  s_display_update_in_progress = false;
  prv_handle_display_update_complete(true /* success */);
  cl_assert_equal_i(s_app_window_render_count, 0);
  cl_assert_equal_i(s_count_animation_destroy, 1);

  // Now the app has rendered something and we should actually update the display.
  compositor_app_render_ready();
  cl_assert_equal_i(s_app_window_render_count, 1);
  cl_assert_equal_i(s_count_animation_destroy, 1);
}

void test_compositor__cancel_modal_to_app_with_another_modal(void) {
  // Show a modal window
  s_modal_window_present = true;
  compositor_transition(nullptr);
  cl_assert_equal_i(s_count_display_update, 1);

  // Now pop it with a transition
  s_modal_window_present = false;
  compositor_transition(&s_transition_a);
  cl_assert_equal_i(s_count_animation_create, 1);
  cl_assert_equal_i(s_count_display_update, 1);
  cl_assert_equal_i(s_count_animation_schedule, 0);

  // Have the app render once so the animation can start
  s_render_pending = true;
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_animation_schedule, 1);
  // Don't allow the app to render while we're animating to it
  cl_assert_equal_i(s_render_pending, true);

  // Now before the previous animation completes, transition to a different modal
  s_modal_window_present = true;
  compositor_transition(&s_transition_b);
  // Create and schedule a new modal animation, destroying the old one
  cl_assert_equal_i(s_count_animation_create, 2);
  cl_assert_equal_i(s_count_animation_schedule, 2);
  cl_assert_equal_i(s_count_animation_destroy, 1);
  // The app framebuffer should still be locked while we're animating away
  cl_assert_equal_i(s_render_pending, true);

  // Finish the animation
  animation_set_implementation_fake.arg1_val->teardown(
      (Animation *)(uintptr_t)s_count_animation_create);
  // App should be free to render again
  cl_assert_equal_i(s_render_pending, false);
}

void test_compositor__freeze_idle_display(void) {
  int data;
  compositor_freeze(prv_frozen_cb, &data);
  // Nothing happens until the freeze runs on KernelMain
  cl_assert_equal_i(s_count_frozen, 0);
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_display_update, 1);

  // No display update in flight: the callback fires as soon as the freeze takes effect
  prv_run_launcher_callback();
  cl_assert_equal_i(s_count_frozen, 1);
  cl_assert_equal_p(s_frozen_data, &data);

  // Frames are held back while frozen
  compositor_app_render_ready();
  cl_assert_equal_i(s_count_display_update, 1);

  // ... and flushed once unfrozen
  compositor_unfreeze();
  prv_run_launcher_callback();
  cl_assert_equal_i(s_count_display_update, 2);
  cl_assert_equal_i(s_count_frozen, 1);
}

void test_compositor__freeze_waits_for_display_update(void) {
  s_display_update_in_progress = true;
  compositor_freeze(prv_frozen_cb, nullptr);
  prv_run_launcher_callback();
  // The framebuffer is still being pushed to the display, so it isn't stable yet
  cl_assert_equal_i(s_count_frozen, 0);

  compositor_app_render_ready();
  cl_assert_equal_i(s_count_display_update, 0);

  // The update completes: the deferred frame stays held back and the callback fires
  s_display_update_in_progress = false;
  prv_handle_display_update_complete(true /* success */);
  cl_assert_equal_i(s_count_display_update, 0);
  cl_assert_equal_i(s_count_frozen, 1);

  compositor_unfreeze();
  prv_run_launcher_callback();
  cl_assert_equal_i(s_count_display_update, 1);
  cl_assert_equal_i(s_count_frozen, 1);
}
