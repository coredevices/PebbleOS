/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "scroll_layer.h"
#include "scroll_layer_private.h"

#include "applib/applib_malloc.auto.h"
#include "applib/graphics/gtypes.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/content_indicator_private.h"
#include "applib/ui/shadows.h"
#include "applib/ui/window.h"
#include "process_management/app_manager.h"
#include <pbl/logging/logging.h>
#include "system/passert.h"
#include "pbl/util/math.h"

#include "animation_timing.h"

#include <string.h>

#ifdef CONFIG_TOUCH
#include "applib/ui/recognizer/touch_nav.h"
#include "applib/ui/recognizer/recognizer_manager.h"
#include "applib/ui/recognizer/pan.h"
#include "applib/ui/recognizer/swipe.h"
#include "kernel/pebble_tasks.h"
#include "pbl/drivers/rtc.h"

// Provided by the owning task; forward-declared (as in menu_layer.c) to avoid pulling kernel
// app-state / modal-manager headers into this applib translation unit.
struct TouchNavState *app_state_get_touch_nav_state(void);
struct TouchNavState *modal_manager_get_touch_nav_state(void);
#endif

T_STATIC bool prv_scroll_layer_is_paging_enabled(ScrollLayer *scroll_layer) {
  PBL_ASSERTN(scroll_layer);
  if (process_manager_compiled_with_legacy2_sdk() || !scroll_layer->shadow_sublayer.hidden) {
    return false;
  }
  return !scroll_layer->paging.paging_disabled;
}

T_STATIC uint16_t prv_scroll_layer_get_paging_height(ScrollLayer *scroll_layer) {
  if (!prv_scroll_layer_is_paging_enabled(scroll_layer)) {
    return 0;
  }
  return MAX(0, scroll_layer->layer.frame.size.h);
}

//! Return callback_context or if NULL,
inline static void* get_callback_context(ScrollLayer *scroll_layer) {
  return scroll_layer->context ? scroll_layer->context : scroll_layer;
}

void scroll_layer_draw_shadow_sublayer(Layer *shadow_sublayer, GContext* ctx) {
  ScrollLayer *scroll_layer = (ScrollLayer *)(((uint8_t*)shadow_sublayer) - offsetof(ScrollLayer, shadow_sublayer));
  const GPoint content_offset = scroll_layer_get_content_offset(scroll_layer);
  const GSize content_size = scroll_layer_get_content_size(scroll_layer);
  const GSize frame_size = scroll_layer->layer.frame.size;
  GBitmap *shadow_top = shadow_get_top();
  GBitmap *shadow_bottom = shadow_get_bottom();

  graphics_context_set_compositing_mode(ctx, GCompOpClear);

  // Draw top shadow, if (partially) visible:
  const int16_t layer_w = shadow_sublayer->bounds.size.w;
  const int16_t layer_h = shadow_sublayer->bounds.size.h;
  const int16_t shadow_top_bitmap_h = shadow_top->bounds.size.h;
  const int16_t shadow_top_y_offset = -shadow_top_bitmap_h - CLIP(content_offset.y, -shadow_top_bitmap_h, 0);
  if (shadow_top_y_offset >= -shadow_top_bitmap_h) {
    graphics_draw_bitmap_in_rect(ctx, shadow_top, &GRect(0, shadow_top_y_offset,
                                                         layer_w, shadow_top_bitmap_h));
  }
  // Draw bottom shadow, if (partially) visible:
  const int16_t shadow_bottom_bitmap_h = shadow_top->bounds.size.h;
  const int16_t bottom_clipped_height = (content_size.h + content_offset.y) - frame_size.h;
  const int16_t shadow_bottom_y_offset = - CLIP(bottom_clipped_height, 0, shadow_bottom_bitmap_h);
  if (shadow_bottom_y_offset < 0) {
    graphics_draw_bitmap_in_rect(ctx, shadow_bottom, &GRect(0, layer_h + shadow_bottom_y_offset,
                                                            layer_w, shadow_bottom_bitmap_h));
  }
}

static void prv_setup_shadow_layer(ScrollLayer *scroll_layer) {
  layer_init(&scroll_layer->shadow_sublayer, &scroll_layer->layer.bounds);
  layer_set_clips(&scroll_layer->shadow_sublayer, true);
  scroll_layer->shadow_sublayer.update_proc = scroll_layer_draw_shadow_sublayer;
  layer_add_child(&scroll_layer->layer, &scroll_layer->shadow_sublayer);
}

static void scroll_layer_property_changed_proc(Layer *layer) {
  ScrollLayer *scroll_layer = (ScrollLayer*)layer;
  const GRect internal_rect = (GRect) { GPointZero, scroll_layer->layer.frame.size };

  // If shadow_sublayer initialized (opposite of paging_enabled)
  if (!prv_scroll_layer_is_paging_enabled(scroll_layer)) {
    scroll_layer->shadow_sublayer.frame = internal_rect;
    scroll_layer->shadow_sublayer.bounds = internal_rect;
  }

  layer_set_frame(&scroll_layer->content_sublayer, &internal_rect);
}

#ifdef CONFIG_TOUCH
// ---------------------------------------------------------------------------------------------
// Tier-1 touch navigation
//
// A ScrollLayer registers itself as a Tier-1 touch widget in scroll_layer_init(). Like MenuLayer,
// the recognizers and gesture state are NOT owned per-widget: a single (pan, swipe) set lives per
// task and drives whichever registered ScrollLayer the finger lands on. On a Tier-1 route the
// per-task system set is failed by the bridge, so this set wins the gesture; on any other route it
// resolves no target and stays inert. A ScrollLayer is a pure scroll container (no selection model):
// a vertical pan drags the content 1:1 and a horizontal swipe navigates (right = BACK, left =
// SELECT). A ScrollLayer embedded in a composite that drives its own scrolling (e.g. MenuLayer)
// deregisters via scroll_layer_touch_nav_deregister() so the same layer never has two gesture
// drivers.

// Minimum wall-clock spacing between live-scroll updates applied from pan Updated events.
#define SCROLL_PAN_UPDATE_MIN_INTERVAL_MS 16

_Static_assert(sizeof(((ScrollLayer *)0)->touch_nav_node) == sizeof(TouchNavWidgetNode),
               "ScrollLayer touch_nav_node must match TouchNavWidgetNode layout");

// Per-task singleton: recognizer set + gesture state. Selected by task so the app and the kernel
// (modal) twins never share state. Slot 0 = KernelMain/other, slot 1 = the app task.
typedef struct ScrollTouchNav {
  bool initialized;
  RecognizerManager *manager;   //!< Manager the recognizers are attached to (this task's).
  ScrollLayer *target;          //!< ScrollLayer currently driving the gesture, or NULL when idle.
  GPoint base;                  //!< Content offset captured when the pan Started.
  RtcTicks last_update_ticks;   //!< Throttle stamp for live-scroll updates.
  _Alignas(void *) uint8_t pan_storage[PAN_RECOGNIZER_STATIC_SIZE];
  _Alignas(void *) uint8_t swipe_storage[SWIPE_RECOGNIZER_STATIC_SIZE];
  Recognizer *pan;
  Recognizer *swipe;
} ScrollTouchNav;

static ScrollTouchNav s_scroll_touch_nav[2];

static bool prv_is_app_task(void) {
  return pebble_task_get_current() == PebbleTask_App;
}

static ScrollTouchNav *prv_task_scroll_touch_nav(void) {
  return &s_scroll_touch_nav[prv_is_app_task() ? 1 : 0];
}

static TouchNavState *prv_task_touch_nav_state(void) {
  return prv_is_app_task() ? app_state_get_touch_nav_state() : modal_manager_get_touch_nav_state();
}

void scroll_layer_touch_nav_reset_all(void) {
  for (unsigned i = 0; i < 2; i++) {
    s_scroll_touch_nav[i] = (ScrollTouchNav){0};
  }
}

bool scroll_layer_touch_is_gesture_target(const ScrollLayer *scroll_layer) {
  return prv_task_scroll_touch_nav()->target == scroll_layer;
}

// Coarse clamp: [min(frame_h - content_h, 0), 0]. With content shorter than the viewport the lower
// bound collapses to 0 (via the min()), so a short page cannot be dragged off its top.
static int16_t prv_scroll_touch_clamp_offset_y(ScrollLayer *scroll_layer, int16_t y) {
  const int16_t frame_h = scroll_layer->layer.frame.size.h;
  const int16_t content_h = scroll_layer_get_content_size(scroll_layer).h;
  const int16_t min_y = MIN((int16_t)(frame_h - content_h), (int16_t)0);
  return CLIP(y, min_y, (int16_t)0);
}

// ---------------------------------------------------------------------------------------------
// Gesture handlers (also the unit-test entry surface)

void scroll_layer_touch_handle_pan_update(ScrollLayer *scroll_layer, GPoint base,
                                          GPoint delta_since_start) {
  const int16_t new_y = prv_scroll_touch_clamp_offset_y(scroll_layer, base.y + delta_since_start.y);
  scroll_layer_set_content_offset(scroll_layer, GPoint(0, new_y), false);
}

void scroll_layer_touch_handle_swipe(ScrollLayer *scroll_layer, SwipeDirection direction) {
  (void)scroll_layer;
  // Horizontal swipe navigation, mirroring the Tier-2 bridge and MenuLayer: right = BACK (pop the
  // top window when it has no back handler), left = SELECT. Guarded against a mid-transition drop.
  const TouchNavState *state = prv_task_touch_nav_state();
  if (!state || !state->ops) {
    return;
  }
  const TouchNavOps *ops = state->ops;
  if (ops->is_animating && ops->is_animating(ops->ctx)) {
    return;
  }
  switch (direction) {
    case SwipeDirection_Right:
      if (!(ops->top_overrides_back && ops->top_overrides_back(ops->ctx))) {
        if (ops->pop_top) {
          ops->pop_top(ops->ctx);
        }
      } else if (ops->emit_button) {
        ops->emit_button(ops->ctx, BUTTON_ID_BACK);
      }
      break;
    case SwipeDirection_Left:
      if (ops->emit_button) {
        ops->emit_button(ops->ctx, BUTTON_ID_SELECT);
      }
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------------------------------------
// Recognizer wiring

// Resolve the registered ScrollLayer under the manager's active layer, or NULL if the current
// gesture is not on a bare scroll layer. Walks parents from the active layer and matches against
// the Scroll registry.
static ScrollLayer *prv_scroll_touch_resolve_target(ScrollTouchNav *stn) {
  if (!stn->manager) {
    return NULL;
  }
  TouchNavState *state = prv_task_touch_nav_state();
  if (!state) {
    return NULL;
  }
  for (Layer *layer = stn->manager->active_layer; layer; layer = layer->parent) {
    for (TouchNavWidgetNode *node = state->scroll_head; node; node = node->next) {
      if (node->layer == layer) {
        // The registry node's layer is the ScrollLayer's root layer, which is the first member of
        // ScrollLayer, so the layer pointer is also the ScrollLayer pointer.
        return (ScrollLayer *)layer;
      }
    }
  }
  return NULL;
}

static ScrollLayer *prv_scroll_touch_ensure_target(ScrollTouchNav *stn) {
  if (!stn->target) {
    stn->target = prv_scroll_touch_resolve_target(stn);
  }
  return stn->target;
}

static void prv_scroll_touch_recognizer_event(const Recognizer *recognizer, RecognizerEvent event) {
  ScrollTouchNav *stn = recognizer_get_user_data(recognizer);
  if (!stn) {
    return;
  }
  switch (event) {
    case RecognizerEvent_Started: {
      ScrollLayer *target = prv_scroll_touch_ensure_target(stn);
      if (target && recognizer == stn->pan) {
        // Touchdown-equivalent for the pan: stop any running scroll animation so the finger takes
        // over from a fling/settle cleanly, then latch the base offset.
        Animation *anim = property_animation_get_animation(target->animation);
        if (anim && animation_is_scheduled(anim)) {
          animation_unschedule(anim);
        }
        stn->base = scroll_layer_get_content_offset(target);
        stn->last_update_ticks = 0;
      }
      break;
    }
    case RecognizerEvent_Updated: {
      ScrollLayer *target = prv_scroll_touch_ensure_target(stn);
      if (target && recognizer == stn->pan) {
        const RtcTicks now = rtc_get_ticks();
        const RtcTicks min_ticks =
            (RtcTicks)SCROLL_PAN_UPDATE_MIN_INTERVAL_MS * RTC_TICKS_HZ / 1000;
        if (now - stn->last_update_ticks >= min_ticks) {
          stn->last_update_ticks = now;
          scroll_layer_touch_handle_pan_update(
              target, stn->base, pan_recognizer_get_delta_since_start((Recognizer *)recognizer));
        }
      }
      break;
    }
    case RecognizerEvent_Completed: {
      ScrollLayer *target = prv_scroll_touch_ensure_target(stn);
      if (target) {
        if (recognizer == stn->pan) {
          scroll_layer_touch_handle_pan_update(
              target, stn->base, pan_recognizer_get_delta_since_start((Recognizer *)recognizer));
        } else if (recognizer == stn->swipe) {
          scroll_layer_touch_handle_swipe(
              target, swipe_recognizer_get_direction((Recognizer *)recognizer));
        }
      }
      stn->target = NULL;
      break;
    }
    case RecognizerEvent_Cancelled: {
      // The content is already settled from the last Updated; a cancelled pan has nothing to undo.
      stn->target = NULL;
      break;
    }
  }
}

static void prv_scroll_touch_nav_register(ScrollLayer *scroll_layer) {
  TouchNavState *state = prv_task_touch_nav_state();
  if (!state || !state->manager) {
    return;
  }

  // The registry add dedups by address and restores the node's layer, so a repeated init on the
  // same scroll layer (no intervening deinit) keeps routing to it. The shared recognizer set is
  // attached the first time any scroll layer on this task registers and stays attached until the
  // registry drains (see the deregister path).
  touch_nav_registry_add(state, TouchNavWidgetType_Scroll,
                         (TouchNavWidgetNode *)&scroll_layer->touch_nav_node, &scroll_layer->layer);

  ScrollTouchNav *stn = prv_task_scroll_touch_nav();
  if (!stn->initialized) {
    stn->manager = state->manager;
    stn->target = NULL;
    stn->pan = pan_recognizer_init_static(stn->pan_storage, prv_scroll_touch_recognizer_event, stn,
                                          PanAxis_Vertical);
    stn->swipe = swipe_recognizer_init_static(stn->swipe_storage, prv_scroll_touch_recognizer_event,
                                              stn, SwipeDirection_Left | SwipeDirection_Right);
    recognizer_add_to_list(stn->pan, state->manager->global_list);
    recognizer_add_to_list(stn->swipe, state->manager->global_list);
    stn->initialized = true;
  }
}

void scroll_layer_touch_nav_deregister(ScrollLayer *scroll_layer) {
  ScrollTouchNav *stn = prv_task_scroll_touch_nav();
  if (stn->target == scroll_layer) {
    // This widget is the live gesture target and is about to go away under a live window: cancel the
    // gesture with NO client callbacks so a settle cannot reach freed state. Clear the target first
    // so the resulting Cancelled event does not re-enter a handler on this widget.
    stn->target = NULL;
    if (stn->manager) {
      recognizer_manager_cancel_and_reset(stn->manager);
    }
  }

  TouchNavState *state = prv_task_touch_nav_state();
  if (!state) {
    return;
  }
  // Idempotent: removing a node that is not in the registry (double deinit, or a never-registered
  // layer) is a safe no-op.
  touch_nav_registry_remove(state, TouchNavWidgetType_Scroll,
                            (TouchNavWidgetNode *)&scroll_layer->touch_nav_node);

  // Detach the shared recognizer set once no scroll layer remains registered on this task.
  if (state->scroll_head == NULL && stn->initialized && stn->manager && stn->manager->global_list) {
    recognizer_remove_from_list(stn->pan, stn->manager->global_list);
    recognizer_remove_from_list(stn->swipe, stn->manager->global_list);
    stn->initialized = false;
    stn->manager = NULL;
  }
}
#endif  // CONFIG_TOUCH

void scroll_layer_init(ScrollLayer *scroll_layer, const GRect *frame) {
  *scroll_layer = (ScrollLayer){};

  layer_init(&scroll_layer->layer, frame);
  const GRect *bounds = &scroll_layer->layer.bounds;
  scroll_layer->layer.property_changed_proc = scroll_layer_property_changed_proc;

  layer_init(&scroll_layer->content_sublayer, bounds);
  layer_add_child(&scroll_layer->layer, &scroll_layer->content_sublayer);

  prv_setup_shadow_layer(scroll_layer);

#ifdef CONFIG_TOUCH
  prv_scroll_touch_nav_register(scroll_layer);
#endif
}

ScrollLayer* scroll_layer_create(GRect frame) {
  ScrollLayer *layer = applib_type_malloc(ScrollLayer);
  if (layer) {
    scroll_layer_init(layer, &frame);
  }
  return layer;
}

bool scroll_layer_is_instance(const Layer *layer) {
  return layer && layer->property_changed_proc == scroll_layer_property_changed_proc;
}

void scroll_layer_deinit(ScrollLayer *scroll_layer) {
#ifdef CONFIG_TOUCH
  // Deregister from the Tier-1 touch registry first so a gesture in flight on this widget is
  // cancelled with no client callbacks before its state is torn down (double-deinit safe).
  scroll_layer_touch_nav_deregister(scroll_layer);
#endif
  animation_destroy(property_animation_get_animation(scroll_layer->animation));
  content_indicator_destroy_for_scroll_layer(scroll_layer);
  layer_deinit(&scroll_layer->layer);
}

void scroll_layer_destroy(ScrollLayer *scroll_layer) {
  if (scroll_layer == NULL) {
    return;
  }
  scroll_layer_deinit(scroll_layer);
  applib_free(scroll_layer);
}

Layer* scroll_layer_get_layer(const ScrollLayer *scroll_layer) {
  return &((ScrollLayer *)scroll_layer)->layer;
}

void scroll_layer_set_frame(ScrollLayer *scroll_layer, GRect rect) {
  scroll_layer->layer.frame = rect;
  layer_mark_dirty(&scroll_layer->layer);
}

void scroll_layer_add_child(ScrollLayer *scroll_layer, Layer *child) {
  layer_add_child(&scroll_layer->content_sublayer, child);
}


GPoint scroll_layer_get_content_offset(ScrollLayer *scroll_layer) {
  return scroll_layer->content_sublayer.bounds.origin;
}

T_STATIC void prv_scroll_layer_set_content_offset_internal(
    ScrollLayer *scroll_layer, GPoint offset) {
  const GSize frame_size = scroll_layer->layer.frame.size;
  GRect bounds = scroll_layer->content_sublayer.bounds;
  const GPoint old_offset = bounds.origin;
  const int16_t min_x_offset = frame_size.w - bounds.size.w;
  int16_t min_y_offset = frame_size.h - bounds.size.h;

  if (prv_scroll_layer_is_paging_enabled(scroll_layer)) {
    uint16_t page_height = prv_scroll_layer_get_paging_height(scroll_layer);
    if (page_height) {
      // showing full page-aligned contents of last page
      min_y_offset = ROUND_TO_MOD_CEIL(min_y_offset, page_height);
    }
  }

  if (scroll_layer_get_clips_content_offset(scroll_layer)) {
    bounds.origin.x = CLIP(offset.x, MIN(min_x_offset, 0), 0);
    bounds.origin.y = CLIP(offset.y, MIN(min_y_offset, 0), 0);
  } else {
    bounds.origin = offset;
  }

  if (gpoint_equal(&old_offset, &bounds.origin)) {
    // Not changed.
    // still, call update_content indicator to refresh potential timers
    scroll_layer_update_content_indicator(scroll_layer);

    return;
  }

  layer_set_bounds(&scroll_layer->content_sublayer, &bounds);
  scroll_layer_update_content_indicator(scroll_layer);

  if (scroll_layer->callbacks.content_offset_changed_handler) {
    scroll_layer->callbacks.content_offset_changed_handler(scroll_layer, get_callback_context(scroll_layer));
  }
}

void scroll_layer_set_content_offset(ScrollLayer *scroll_layer, GPoint offset, bool animated) {
  // Note: animation_is_scheduled() returns false and property_animation_destroy does nothing
  // if the argument is NULL
  Animation *animation = property_animation_get_animation(scroll_layer->animation);
  bool was_running = false;
  if (animation) {
    was_running = animation_is_scheduled(animation);
    if (was_running) {
      animation_unschedule(animation);
    }
  }
  if (animated) {
    static const PropertyAnimationImplementation implementation = {
      .base = {
        .update = (AnimationUpdateImplementation) property_animation_update_gpoint,
      },
      .accessors = {
        .setter = { .grect = (const GRectSetter) (void *) prv_scroll_layer_set_content_offset_internal, },
        .getter = { .grect = (const GRectGetter) (void *) scroll_layer_get_content_offset, },
      },
    };
    if (animation) {
      property_animation_init(scroll_layer->animation, &implementation, scroll_layer, NULL,
                              &offset);
      if (was_running && !scroll_layer_get_paging(scroll_layer)) {
        animation_set_curve(animation, AnimationCurveEaseOut);
      }
    } else {
      scroll_layer->animation = property_animation_create(&implementation, scroll_layer, NULL,
                                                          &offset);
      animation = property_animation_get_animation(scroll_layer->animation);
      if (scroll_layer_get_paging(scroll_layer)) {
        animation_set_custom_interpolation(animation, interpolate_moook);
        animation_set_duration(animation, interpolate_moook_duration());
      }
      animation_set_auto_destroy(animation, false);
    }
    animation_schedule(animation);
  } else {
    prv_scroll_layer_set_content_offset_internal(scroll_layer, offset);
  }
}

void scroll_layer_set_content_size(ScrollLayer *scroll_layer, GSize size) {
  GRect bounds = scroll_layer->content_sublayer.bounds;
  bounds.size = size;
  layer_set_bounds(&scroll_layer->content_sublayer, &bounds);
  // Ensure our content offset is clipped to the new size.
  // We call prv_scroll_layer_set_content_offset_internal() directly and
  // keep potential animations running – since some 3rd-party apps do change the content size
  // frequently (e.g. in an update_proc) and would otherwise implicitly stop scroll animations.
  // It's fine to keep scroll animations running as they clip the offset to valid bounds.
  prv_scroll_layer_set_content_offset_internal(
      scroll_layer, scroll_layer_get_content_offset(scroll_layer));
}

GSize scroll_layer_get_content_size(const ScrollLayer *scroll_layer) {
  return scroll_layer->content_sublayer.bounds.size;
}

void scroll_layer_scroll(ScrollLayer *scroll_layer, ScrollDirection direction, bool animated) {
  GPoint offset = scroll_layer_get_content_offset(scroll_layer);
  int32_t scroll_height = 32;

  // If process is 3.x and has enabled paging
  if (prv_scroll_layer_is_paging_enabled(scroll_layer)) {
    uint16_t page_height = prv_scroll_layer_get_paging_height(scroll_layer);
    if (page_height) {
      // Force offset to start (and stay) page aligned
      offset.y = ROUND_TO_MOD_CEIL(offset.y, page_height);
      scroll_height = page_height;
    }
  }

  switch (direction) {
    case ScrollDirectionUp:
      offset.y += scroll_height;
      break;
    case ScrollDirectionDown:
      offset.y -= scroll_height;
      break;

    default: return;
  }
  scroll_layer_set_content_offset(scroll_layer, offset, animated);
}

void scroll_layer_scroll_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  ScrollLayer *scroll_layer = (ScrollLayer *)context;
  scroll_layer_scroll(scroll_layer, ScrollDirectionUp, true);
  (void)recognizer;
}

void scroll_layer_scroll_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  ScrollLayer *scroll_layer = (ScrollLayer *)context;
  scroll_layer_scroll(scroll_layer, ScrollDirectionDown, true);
  (void)recognizer;
}

static void scroll_layer_click_config_provider(ScrollLayer *scroll_layer) {
  // Config UP / DOWN button behavior:
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, scroll_layer_scroll_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, scroll_layer_scroll_down_click_handler);
  // Set the context for the SELECT button:
  window_set_click_context(BUTTON_ID_SELECT, get_callback_context(scroll_layer));

  // Callback to provide the client to setup the SELECT button:
  if (scroll_layer->callbacks.click_config_provider) {
    scroll_layer->callbacks.click_config_provider(get_callback_context(scroll_layer));
  }
}

void scroll_layer_set_click_config_onto_window(ScrollLayer *scroll_layer, struct Window *window) {
  window_set_click_config_provider_with_context(window, (ClickConfigProvider) scroll_layer_click_config_provider, scroll_layer);
}

void scroll_layer_set_callbacks(ScrollLayer *scroll_layer, ScrollLayerCallbacks callbacks) {
  scroll_layer->callbacks = callbacks;
}

void scroll_layer_set_context(ScrollLayer *scroll_layer, void *context) {
  scroll_layer->context = context;
}

void scroll_layer_set_shadow_hidden(ScrollLayer *scroll_layer, bool hidden) {
  PBL_ASSERTN(scroll_layer);

  // paging and shadow_sublayer are mutually exclusive
  // so init shadow_sublayer if it was paging data
  if (prv_scroll_layer_is_paging_enabled(scroll_layer) && hidden == false) {
    prv_setup_shadow_layer(scroll_layer);
  }

  scroll_layer_property_changed_proc((Layer*)scroll_layer);
  layer_set_hidden(&scroll_layer->shadow_sublayer, hidden);
}

bool scroll_layer_get_shadow_hidden(const ScrollLayer *scroll_layer) {
  return layer_get_hidden(&scroll_layer->shadow_sublayer);
}

void scroll_layer_set_paging(ScrollLayer *scroll_layer, bool paging_enabled) {
  PBL_ASSERTN(scroll_layer);
  if (paging_enabled) {
    // Deinit shadow_sublayer to enable paging
    if (!prv_scroll_layer_is_paging_enabled(scroll_layer)) {
      layer_deinit(&scroll_layer->shadow_sublayer);
    }

    // paging and shadow_sublayer are mutually exclusive
    scroll_layer->paging.shadow_hidden = true;
    scroll_layer->paging.paging_disabled = false;
  } else {
    if (prv_scroll_layer_is_paging_enabled(scroll_layer)) {
      prv_setup_shadow_layer(scroll_layer);
      // still require explicit un-hiding of shadow
      scroll_layer_set_shadow_hidden(scroll_layer, true);
    }
  }
}

bool scroll_layer_get_paging(ScrollLayer* scroll_layer) {
  return scroll_layer && prv_scroll_layer_is_paging_enabled(scroll_layer);
}

ContentIndicator *scroll_layer_get_content_indicator(ScrollLayer *scroll_layer) {
  return content_indicator_get_or_create_for_scroll_layer(scroll_layer);
}

void scroll_layer_update_content_indicator(ScrollLayer *scroll_layer) {
  ContentIndicator *content_indicator = content_indicator_get_for_scroll_layer(scroll_layer);
  if (!content_indicator) {
    return;
  }

  const GSize scroll_layer_frame_size = scroll_layer_get_layer(scroll_layer)->frame.size;
  const GSize scroll_layer_content_size = scroll_layer_get_content_size(scroll_layer);
  const int16_t scroll_layer_content_offset_y = scroll_layer_get_content_offset(scroll_layer).y;

  const bool content_available_up = (scroll_layer_content_offset_y < 0);
  content_indicator_set_content_available(content_indicator,
                                          ContentIndicatorDirectionUp,
                                          content_available_up);
  const bool content_available_down =
    (scroll_layer_frame_size.h - scroll_layer_content_offset_y < scroll_layer_content_size.h);
  content_indicator_set_content_available(content_indicator,
                                          ContentIndicatorDirectionDown,
                                          content_available_down);
}

void scroll_layer_set_clips_content_offset(ScrollLayer *scroll_layer, bool clips) {
  scroll_layer->content_sublayer.clips = clips;
  scroll_layer_set_content_offset(scroll_layer,
                                  scroll_layer_get_content_offset(scroll_layer), false);
}

bool scroll_layer_get_clips_content_offset(ScrollLayer *scroll_layer) {
  return scroll_layer->content_sublayer.clips;
}
