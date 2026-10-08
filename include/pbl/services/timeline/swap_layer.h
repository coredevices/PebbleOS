/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <applib/ui/layer.h>
#include <applib/ui/click.h>
#include <applib/ui/property_animation.h>
#include <pbl/services/timeline/layout_layer.h>

/**
 * @defgroup services_timeline_swap_layer Swap layer
 * @ingroup services_timeline
 * @brief Scrolls through a sequence of layouts, swapping to the previous or next at the ends.
 *
 * Used by the notification window. The SwapLayer holds the previous, current and next layouts,
 * which the client supplies on demand through @ref SwapLayerCallbacks::get_layout_handler and
 * destroys in @ref SwapLayerCallbacks::layout_removed_handler. Up and down scroll the current
 * layout and swap to the neighbor once its end is reached.
 * @{
 */

struct Window;
struct SwapLayer;

/**
 * @brief Get a layout relative to the current one.
 *
 * @param swap_layer Swap layer.
 * @param rel_position Offset from the current layout, which the client keeps track of: -1 for the
 *                     previous, 0 for the current, 1 for the next.
 * @param context Callback context.
 * @return Layout, or NULL if there is none at that position.
 */
typedef LayoutLayer *(*SwapLayerGetLayoutHandler)(struct SwapLayer *swap_layer, int8_t rel_position,
                                                  void *context);

/**
 * @brief Called when the swap layer no longer uses a layout; the client may destroy it.
 *
 * @param swap_layer Swap layer.
 * @param layer Removed layout.
 * @param context Callback context.
 */
typedef void (*SwapLayerLayoutRemovedHandler)(struct SwapLayer *swap_layer, LayoutLayer *layer,
                                              void *context);

/**
 * @brief Called when a layout has become the current one.
 *
 * @param swap_layer Swap layer.
 * @param layer New current layout.
 * @param rel_change Offset from the previous current layout.
 * @param context Callback context.
 */
typedef void (*SwapLayerLayoutDidAppearHandler)(struct SwapLayer *swap_layer, LayoutLayer *layer,
                                                int8_t rel_change, void *context);

/**
 * @brief Called when a layout is about to become the current one.
 *
 * @param swap_layer Swap layer.
 * @param layer Upcoming layout.
 * @param context Callback context.
 */
typedef void (*SwapLayerLayoutWillAppearHandler)(struct SwapLayer *swap_layer, LayoutLayer *layer,
                                                 void *context);

/**
 * @brief Called when the window colors should change to match the visible layout.
 *
 * @param swap_layer Swap layer.
 * @param bg_color Background color.
 * @param status_bar_filled Whether the status bar should be filled with @p bg_color.
 * @param context Callback context.
 */
typedef void (*SwapLayerUpdateColorsHandler)(struct SwapLayer *swap_layer, GColor bg_color,
                                             bool status_bar_filled, void *context);

/**
 * @brief Called on every user interaction, e.g. to restart an inactivity timer.
 *
 * @param swap_layer Swap layer.
 * @param context Callback context.
 */
typedef void (*SwapLayerInteractionHandler)(struct SwapLayer *swap_layer, void *context);

/**
 * @brief Callbacks of a SwapLayer.
 *
 * @note The context set with swap_layer_set_callbacks() is passed to all of them.
 */
typedef struct {
  /** Supplies layouts. */
  SwapLayerGetLayoutHandler get_layout_handler;
  /** Releases layouts. */
  SwapLayerLayoutRemovedHandler layout_removed_handler;
  /** Called after a swap. */
  SwapLayerLayoutDidAppearHandler layout_did_appear_handler;
  /** Called before a swap. */
  SwapLayerLayoutWillAppearHandler layout_will_appear_handler;
  /** Updates the window colors. */
  SwapLayerUpdateColorsHandler update_colors_handler;
  /** Reports user interaction. */
  SwapLayerInteractionHandler interaction_handler;
  /** Adds click handlers on top of the swap layer's own, may be NULL. */
  ClickConfigProvider click_config_provider;
} SwapLayerCallbacks;

/** @brief Layer drawing the arrow hinting at more content. */
typedef struct {
  /** Layer. */
  Layer layer;
  /** Arrow image. */
  GBitmap arrow_bitmap;
} ArrowLayer;

/**
 * @brief Data structure of a SwapLayer.
 *
 * @note A SwapLayer can be cast to a Layer and used with all functions that take one, e.g.
 * @code{.c}
 * SwapLayer swap_layer;
 * ...
 * layer_set_hidden((Layer *)&swap_layer, true);
 * @endcode
 */
typedef struct SwapLayer {
  /** Root layer. */
  Layer layer;
  /** Arrow hinting at more content. */
  ArrowLayer arrow_layer;
  /** Scroll or swap animation in progress. */
  Animation *animation;
  /** Previous layout. */
  LayoutLayer *previous;
  /** Current layout. */
  LayoutLayer *current;
  /** Next layout. */
  LayoutLayer *next;
  /** Callbacks. */
  SwapLayerCallbacks callbacks;
  /** Repeated clicks left before a held button swaps to the next layout. */
  uint16_t swap_delay_remaining;
  /** A swap is in progress. */
  bool swap_in_progress;
  /** The swap layer is being deinitialized. */
  bool is_deiniting;
  /** Callback context. */
  void *context;
#ifdef CONFIG_TOUCH
  /** @cond INTERNAL_HIDDEN */
  /*
   * Intrusive Tier-1 touch-navigation registry node. Layout-compatible with TouchNavWidgetNode
   * (four pointers: next, layer, ops, widget); a build-time assert in swap_layer.c keeps it in
   * sync. Declared as opaque pointers so this header does not pull in the recognizer stack.
   */
  struct {
    void *next;
    void *layer;
    void *ops;
    void *widget;
  } touch_nav_node;
  /*
   * True while this SwapLayer is threaded onto the Tier-1 registry (guards double add/remove and
   * makes the init/focus re-registration idempotent).
   */
  bool touch_registered;
  /** @endcond */
#endif
} SwapLayer;

/**
 * @brief Initialize a swap layer, without layouts.
 *
 * @param[out] swap_layer Swap layer.
 * @param frame Frame of the layer.
 */
void swap_layer_init(SwapLayer *swap_layer, const GRect *frame);

/**
 * @brief Deinitialize a swap layer.
 *
 * Calls the layout removed handler for every layout it tracks.
 *
 * @param swap_layer Swap layer.
 */
void swap_layer_deinit(SwapLayer *swap_layer);

/**
 * @brief Reload the layouts.
 *
 * Calls the layout removed handler for every layout it tracks, then fetches the current and next
 * layouts. The will appear and did appear handlers are both called.
 *
 * @param swap_layer Swap layer.
 */
void swap_layer_reload_data(SwapLayer *swap_layer);

/**
 * @brief Get the current layout.
 *
 * @param swap_layer Swap layer.
 * @return Current layout, or NULL.
 */
LayoutLayer *swap_layer_get_current_layout(const SwapLayer *swap_layer);

/**
 * @brief Get the root layer.
 *
 * @param swap_layer Swap layer.
 * @return Root layer.
 */
Layer *swap_layer_get_layer(const SwapLayer *swap_layer);

/**
 * @brief Set the callbacks.
 *
 * @param swap_layer Swap layer.
 * @param callback_context Context passed to the callbacks.
 * @param callbacks Callbacks.
 */
void swap_layer_set_callbacks(SwapLayer *swap_layer, void *callback_context,
                              SwapLayerCallbacks callbacks);

/**
 * @brief Install the swap layer's click handling on a window.
 *
 * @param swap_layer Swap layer.
 * @param window Window receiving the clicks.
 */
void swap_layer_set_click_config_onto_window(SwapLayer *swap_layer, struct Window *window);

/**
 * @brief Try to swap to the layout in a direction.
 *
 * Fails if there is no layout to swap to; on success the will appear and did appear handlers are
 * called.
 *
 * @param swap_layer Swap layer.
 * @param direction Direction to swap in.
 * @return true if the swap started.
 */
bool swap_layer_attempt_layer_swap(SwapLayer *swap_layer, ScrollDirection direction);

#ifdef CONFIG_TOUCH
/**
 * @brief Scroll the current layout by a pixel delta without animation.
 *
 * Used by touch panning for live 1:1 scrolling. The scroll offset is clamped to the scrollable
 * range. The next layout is pulled right under the current one so its peek tracks the finger, and
 * the auto-close inactivity timer is refreshed so a long notification cannot close mid-read.
 *
 * @param swap_layer Swap layer.
 * @param dy Delta in pixels; positive scrolls back towards the top, negative further into the
 *           content.
 */
void swap_layer_touch_scroll_by(SwapLayer *swap_layer, int16_t dy);

/**
 * @brief Release touch input while the swap layer is covered by a higher modal.
 *
 * Removes it from the touch-navigation registry so touch cannot reach the hidden notification
 * body. Idempotent and safe when touch is disabled; the click config provider registers it again
 * when it is shown.
 *
 * @param swap_layer Swap layer.
 */
void swap_layer_touch_release(SwapLayer *swap_layer);

/** @cond INTERNAL_HIDDEN */

/* Test seam: zero the per-task Tier-1 gesture singletons for cross-test isolation. */
void swap_layer_touch_nav_reset_all(void);

/* Test seam: whether swap_layer is the current per-task gesture target. */
bool swap_layer_touch_is_gesture_target(const SwapLayer *swap_layer);

/*
 * Test seam: register/deregister this SwapLayer as a Tier-1 touch widget. Normally driven by
 * swap_layer_init / the click-config-provider / swap_layer_deinit; exposed for unit tests.
 */
void swap_layer_touch_register(SwapLayer *swap_layer);
void swap_layer_touch_deregister(SwapLayer *swap_layer);

/*
 * Test seam: the liftoff decision for a pan that started at base_offset and moved delta_y (finger
 * travel; negative is up). Encodes the two-threshold logic (DRAG_THRESHOLD_PX and
 * SWAP_OVERPULL_PX) independently of the animation/swap machinery.
 */
typedef enum SwapTouchLiftoffAction {
  SwapTouchLiftoff_None,     // Sub-threshold drag: do nothing.
  SwapTouchLiftoff_SwapPrev, // Over-pull past the top: swap to the previous notification.
  SwapTouchLiftoff_SwapNext, // Over-pull past the bottom: swap to the next notification.
  SwapTouchLiftoff_Settle,   // A normal scroll: settle to the clamped offset.
} SwapTouchLiftoffAction;

SwapTouchLiftoffAction swap_layer_touch_liftoff_action(int16_t base_offset, int16_t delta_y,
                                                       int16_t max_dy);

/*
 * Test seam: the offset a Settle liftoff (or a cancelled pan) animates to. The raw target is
 * clamp(base_offset - delta_y, 0, max_dy); with page_h > 0 it is then quantized to the nearest
 * page boundary, never past the last whole page below max_dy (which may include the
 * next-notification peek). Round displays pass LAYOUT_HEIGHT, as the circular text flow is
 * computed for page-aligned rest positions, while rect passes 0 and settles freely.
 */
int16_t swap_layer_touch_settle_offset(int16_t base_offset, int16_t delta_y, int16_t max_dy,
                                       int16_t page_h);

/** @endcond */
#endif // CONFIG_TOUCH

/** @} */
