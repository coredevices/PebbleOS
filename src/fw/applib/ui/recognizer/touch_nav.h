/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pan.h"
#include "recognizer.h"
#include "recognizer_manager.h"
#include "swipe.h"
#include "tap.h"

#include "applib/graphics/gtypes.h"
#include "applib/ui/layer.h"
#include "pbl/drivers/button_id.h"
#include "pbl/services/touch/touch_event.h"

#include <stdbool.h>
#include <stdint.h>

//! Touch-nav routing bridge (per-task).
//!
//! A single (tap, pan, swipe) system recognizer set lives by value here and is registered on the
//! task's global recognizer list. The event callback of the touch service (`touch_nav_dispatch`)
//! resolves a route once per Touchdown, latches it, and excludes tiers via `recognizer_set_failed`
//! at routing time. On completion the bridge emulates a button press. The concrete side effects
//! (window-stack animation query, back-pop, click synthesis, idle refresh) are provided by the
//! owning task through a \ref TouchNavOps vtable so the core is task-agnostic and unit-testable.

//! Height, in pixels, of the top status-bar dead zone. A Touchdown here does not drive the bridge;
//! it is routed to the window's sole Tier-1 widget, or dropped.
#define TOUCH_NAV_STATUS_BAR_DEAD_ZONE_PX (16)

//! Number of entries in the per-task observability ring buffer.
#define TOUCH_NAV_LOG_ENTRIES (16)

//! Route resolved once on Touchdown and latched for the whole gesture.
typedef enum TouchNavRoute {
  //! Nobody: all three system recognizers are failed (bridge disabled, or dead-zone drop).
  TouchNavRoute_None,
  //! A Tier-1 widget owns the gesture; the system set is failed so it does not also emulate.
  TouchNavRoute_Tier1,
  //! Tier-2 bridge: tap/swipe completions emulate buttons; pan is failed.
  TouchNavRoute_Tier2,
  //! The gesture is dropped (status-bar dead zone with no sole widget, or a gated Touchdown).
  TouchNavRoute_Dropped,
} TouchNavRoute;

//! Which Tier-1 registry a widget node belongs to; the registry head implies the type.
typedef enum TouchNavWidgetType {
  TouchNavWidgetType_Menu,
  TouchNavWidgetType_Swap,
} TouchNavWidgetType;

//! 8-byte intrusive registry node embedded in a Tier-1 widget (no size growth on the widget).
//! `layer` identifies the widget for the parent-walk match; `next` links the registry list.
typedef struct TouchNavWidgetNode {
  struct TouchNavWidgetNode *next;
  struct Layer *layer;
} TouchNavWidgetNode;

//! Snapshot of the foreground ActionBarLayer, published from applib via a syscall on
//! add_to_window / set_icon(_animated) / remove_from_window. Read when synthesising a tap so the
//! tap is routed into the bar's UP / SELECT / DOWN zone instead of a plain SELECT.
typedef struct TouchNavActionBar {
  //! Bar geometry in GLOBAL (screen) coordinates.
  GRect frame;
  //! Bit i is set when button (i + 1) carries an icon: bit0 = UP, bit1 = SELECT, bit2 = DOWN.
  //! A tap in a zone whose icon bit is clear falls back to SELECT.
  uint8_t icon_mask;
  //! True while a bar is on the foreground window; false clears the snapshot so no stale bar routes
  //! taps after the owning window/app goes away.
  bool present;
} TouchNavActionBar;

//! Task-specific side effects for the bridge. All pointers are consulted through `ctx`. Any may be
//! NULL, in which case the effect is skipped (e.g. the app task has no idle-timeout refresh).
typedef struct TouchNavOps {
  //! @return true if the task's window stack is mid-transition.
  bool (*is_animating)(void *ctx);
  //! @return true if the top window overrides the back button.
  bool (*top_overrides_back)(void *ctx);
  //! @return true if the top window has the touch bridge disabled.
  bool (*top_bridge_disabled)(void *ctx);
  //! Pop the top window (BACK on a window without a back handler).
  void (*pop_top)(void *ctx);
  //! Synthesize a button down+up pair for \a button on the task's click manager.
  void (*emit_button)(void *ctx, ButtonId button);
  //! Refresh the idle timeout. NULL on tasks without one.
  void (*idle_refresh)(void *ctx);
  void *ctx;
} TouchNavOps;

//! Kinds of ring-buffer log entries.
typedef enum TouchNavLogKind {
  TouchNavLog_Route,      //!< A route was latched (detail = TouchNavRoute).
  TouchNavLog_Emit,       //!< A button was emulated (detail = ButtonId).
  TouchNavLog_Dropped,    //!< A completion was dropped mid-animation (detail = ButtonId).
  TouchNavLog_Gated,      //!< A gated (non-navigational) Touchdown (detail = 0).
} TouchNavLogKind;

typedef struct TouchNavLogEntry {
  uint8_t kind;
  uint8_t detail;
} TouchNavLogEntry;

//! Per-task touch-nav state. Held by value in the task's process state.
typedef struct TouchNavState {
  RecognizerManager *manager;
  const TouchNavOps *ops;

  //! The (tap, pan, swipe) system recognizer set, by value.
  _Alignas(void *) uint8_t tap_storage[TAP_RECOGNIZER_STATIC_SIZE];
  _Alignas(void *) uint8_t pan_storage[PAN_RECOGNIZER_STATIC_SIZE];
  _Alignas(void *) uint8_t swipe_storage[SWIPE_RECOGNIZER_STATIC_SIZE];
  Recognizer *tap;
  Recognizer *pan;
  Recognizer *swipe;

  //! Tier-1 widget registry heads; the type is implied by which head a node hangs off.
  TouchNavWidgetNode *menu_head;
  TouchNavWidgetNode *swap_head;

  //! Route latched on the most recent Touchdown.
  TouchNavRoute route;

  //! Latest foreground ActionBarLayer snapshot; consulted when synthesising a tap.
  TouchNavActionBar action_bar;

  //! Observability counters.
  struct {
    uint16_t started;
    uint16_t completed;
    uint16_t failed;
    uint16_t cancelled;
    uint16_t dropped;
    uint16_t gated;
  } counters;

  //! Observability ring buffer.
  TouchNavLogEntry log[TOUCH_NAV_LOG_ENTRIES];
  uint8_t log_head;   //!< Index of the next slot to write.
  uint8_t log_count;  //!< Number of valid entries (saturates at TOUCH_NAV_LOG_ENTRIES).
} TouchNavState;

//! Initialize the per-task touch-nav state: build the system recognizer set into embedded storage,
//! register it on \a manager's global list, and zero the registry/counters. \a ops must outlive the
//! state.
void touch_nav_state_init(TouchNavState *state, RecognizerManager *manager, const TouchNavOps *ops);

//! Tear the state down: cancel any in-flight gesture and deregister the system recognizer set from
//! the manager's global list. No client callbacks are invoked for a widget-only rebuild.
void touch_nav_state_deinit(TouchNavState *state);

//! Touch-service system-slot handler. Conforms to the touch service handler prototype
//! (\ref TouchServiceHandler). \a context is the \ref TouchNavState.
void touch_nav_dispatch(const TouchEvent *touch_event, void *context);

//! Store the foreground ActionBarLayer snapshot into \a state. A NULL \a frame clears the snapshot
//! (bar removed / no window); otherwise \a frame is the bar's global-coordinate rectangle and
//! \a icon_mask its per-zone icon presence bits. Called from the \ref sys_touch_set_action_bar
//! syscall handler on the task that owns the bar.
void touch_nav_set_action_bar(TouchNavState *state, const GRect *frame, uint8_t icon_mask);

//! Resolve a tap at \a point against the action-bar snapshot \a bar. Returns the zoned button when
//! the bar is present and the point is inside its frame: the frame is split vertically into three
//! equal zones (top = UP, middle = SELECT, bottom = DOWN). A zone whose icon bit is clear, a point
//! outside the frame, or an absent snapshot all fall back to \ref BUTTON_ID_SELECT. Swipes are not
//! zoned; only taps consult this.
ButtonId touch_nav_action_bar_zone_button(const TouchNavActionBar *bar, GPoint point);

//! Register a Tier-1 widget node under the given registry. Dedup-by-address (a re-add WARNs and is
//! a no-op). Robust to a node zeroed by the widget's *_init.
void touch_nav_registry_add(TouchNavState *state, TouchNavWidgetType type, TouchNavWidgetNode *node,
                            struct Layer *layer);

//! Remove a Tier-1 widget node from its registry by predecessor traversal. A node that is not
//! present (e.g. zeroed by *_init and never added) is a safe no-op.
void touch_nav_registry_remove(TouchNavState *state, TouchNavWidgetType type,
                               TouchNavWidgetNode *node);

//! Effects of the master-pref enable/disable transaction. The ordering is mandatory and lives in
//! \ref touch_nav_transaction_apply; the owning shell provides the concrete effects (persist,
//! kernel/app subscription juggling, permanent sensor hold). Split out so the ordering is unit
//! testable independently of the kernel.
typedef struct TouchNavTxnOps {
  //! Persist the new pref value (1) and flip the master nav gate (touch_set_nav_enabled).
  void (*persist)(void *ctx, bool enable);
  //! Enable (2): on KernelMain, subscribe the kernel touch slot.
  void (*kernel_subscribe)(void *ctx);
  //! Enable (2): take the permanent sensor hold (touch_set_system_hold(true)).
  void (*take_system_hold)(void *ctx);
  //! Disable (2): synthesize a Liftoff if a finger is down (backlight+driver only).
  void (*synthesize_liftoff)(void *ctx);
  //! Disable (3): cancel_and_reset the kernel manager and unsubscribe the kernel slot.
  void (*kernel_cancel_reset_unsub)(void *ctx);
  //! Disable (4): send the app-task callback that cancel_and_resets the app manager and drops its
  //! subscription.
  void (*app_unsubscribe)(void *ctx);
  //! Disable (5): release the permanent sensor hold (touch_set_system_hold(false)).
  void (*release_system_hold)(void *ctx);
  void *ctx;
} TouchNavTxnOps;

//! Apply the master-pref transaction in the mandated order. On enable: persist+gate, then (on
//! KernelMain) subscribe the kernel slot and take the sensor hold. On disable: persist+gate, then
//! synthesize a Liftoff, cancel_and_reset+unsubscribe the kernel manager, send the app-unsubscribe
//! callback, and finally release the sensor hold.
void touch_nav_transaction_apply(const TouchNavTxnOps *ops, bool enable);

//! Gate for the app-task touch-nav twin. The twin only installs the system touch handler when the
//! master pref is on AND this app participates in touch navigation. System apps participate by
//! default; third-party apps are inert unless they opt in (\ref app_touch_navigation_enable), so a
//! third-party app never gets touch nav merely because the master pref is on. Factored here (out of
//! app_state.c) so the gate is unit-testable without the kernel app-state singleton.
bool touch_nav_app_twin_active(bool pref_enabled, bool participating);

//! Concrete effects of the app-task touch-nav twin subscription. The owning task provides these so
//! the subscribe/reconcile state machine is unit-testable independently of the kernel app-state
//! singleton (mirrors \ref TouchNavTxnOps). Any op is consulted through \a ctx.
typedef struct TouchNavTwinOps {
  //! @return the master nav pref (touch_nav_enabled()).
  bool (*pref_enabled)(void *ctx);
  //! Install the system touch handler for this task's nav twin (subscribe).
  void (*install_handler)(void *ctx);
  //! Cancel any in-flight gesture and clear the system touch handler (unsubscribe). Safe to call
  //! even when nothing is installed.
  void (*remove_handler)(void *ctx);
  void *ctx;
} TouchNavTwinOps;

//! Install the app twin's touch handler iff the master pref is on AND the app participates
//! (\ref touch_nav_app_twin_active). A no-op otherwise. Safe to call repeatedly.
void touch_nav_app_twin_subscribe(const TouchNavTwinOps *ops, bool participating);

//! Reconcile the twin with a new participation value (the opt-in API path). \a participating points
//! at the caller's stored flag. Idempotent: when the value is unchanged this is a no-op (no
//! double-subscribe, no spurious unsubscribe). On a false->true transition it subscribes (installing
//! only when the pref is on); on true->false it removes the handler.
void touch_nav_app_twin_reconcile(const TouchNavTwinOps *ops, bool *participating, bool enable);
