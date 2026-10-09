/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "gesture_event.h"
#include "touch_event.h"

#include <stdbool.h>

/**
 * @defgroup services_touch Touch
 * @ingroup services
 * @brief Touchscreen input: raw touch and gesture events, navigation gating and injection.
 *
 * Touch drivers report samples (@c PBL_INPUT_BTN_TOUCH, @c PBL_INPUT_ABS_X and Y) and gestures
 * (@c PBL_INPUT_EV_GES) through the @ref input subsystem, which the service passes to
 * touch_handle_update() and touch_handle_gesture(). The service turns them into
 * @c PEBBLE_TOUCH_EVENT (Touchdown, PositionUpdate, Liftoff) and @c PEBBLE_GESTURE_EVENT events,
 * mirroring coordinates in left-hand mode. The sensor is powered while anything holds it: event
 * service subscribers, the touch backlight feature or the system navigation hold, unless touch is
 * globally disabled.
 * @{
 */

/** @brief Finger state reported by the touch driver. */
typedef enum TouchState {
  /** No finger on the screen. */
  TouchState_FingerUp,
  /** A finger is on the screen. */
  TouchState_FingerDown,
} TouchState;

/** @brief Gesture reported by the touch driver. */
typedef enum TouchGesture {
  /** Single tap. */
  TouchGesture_Tap,
  /** Double tap. */
  TouchGesture_DoubleTap,
  TouchGesture_Palm,
} TouchGesture;

/** @brief Initialize the service and register the touch and gesture event types. */
void touch_init(void);

/**
 * @brief Enable or disable the kernel touch subscription used by the touch backlight feature.
 *
 * When disabled, the sensor is only powered while other holders remain.
 *
 * @param enabled true to hold the sensor for the backlight feature.
 */
void touch_set_backlight_enabled(bool enabled);

/**
 * @brief Hold the sensor powered for system touch navigation.
 *
 * Unlike the backlight subscription, this holds the sensor directly, without an event service
 * subscription. Taken when the master navigation pref turns on, released when it turns off.
 *
 * @param held true to take the hold, false to release it.
 */
void touch_set_system_hold(bool held);

/**
 * @brief Check whether system touch navigation is enabled.
 *
 * Off by default; the shell sets it with touch_set_nav_enabled() from the master "Touch" pref
 * and the "Touch Navigation" sub-pref.
 *
 * @return true when system touch navigation is effectively enabled.
 */
bool touch_nav_enabled(void);

/**
 * @brief Set the system navigation gate.
 *
 * Driven by the shell pref system when the effective (master and sub-pref) state changes.
 *
 * @param enabled true to enable system touch navigation.
 */
void touch_set_nav_enabled(bool enabled);

/**
 * @brief Check whether the app navigation dispatcher is installed for the running app.
 *
 * True with system navigation active, or for an opted-in third-party app under the master pref.
 * Cleared as well when the app's touch subscription is torn down, so a dead app cannot leave it
 * set. Feeds the dispatch gate and touch-driven backlight behavior.
 *
 * @return true while the app navigation dispatcher is installed.
 */
bool touch_app_nav_active(void);

/**
 * @brief Mark whether the app navigation dispatcher is installed.
 *
 * @param active true when installed.
 */
void touch_set_app_nav_active(bool active);

/**
 * @brief Check for explicit raw touch subscriptions.
 *
 * Only touch_service_subscribe() subscriptions count: navigation dispatchers, the backlight
 * subscription and the system hold do not.
 *
 * @return true if at least one task holds a raw touch subscription.
 */
bool touch_has_app_subscribers(void);

/**
 * @brief Globally enable or disable touch.
 *
 * When disabled, the sensor is powered down even if subscribers exist, driver samples and
 * gestures are dropped, touch_service_is_enabled() returns false to apps, and an in-progress
 * touch gets a synthetic Liftoff. Subscribers stay subscribed and receive events again when
 * re-enabled. Backs a user setting persisted by the shell, which calls this on boot.
 *
 * @param enabled true to enable touch.
 */
void touch_service_set_globally_enabled(bool enabled);

/**
 * @brief Get the global touch enable flag.
 *
 * @return true if touch is globally enabled.
 */
bool touch_service_is_globally_enabled(void);

/**
 * @brief Pass a touch sample to the service.
 * Emits Touchdown or Liftoff on state changes and PositionUpdate
 * when a down finger moves. Dropped while touch is globally disabled or an injected gesture owns
 * the sensor.
 *
 * @param touch_state Whether the screen is touched.
 * @param x X coordinate in pixels, before left-hand mode mirroring.
 * @param y Y coordinate in pixels, before left-hand mode mirroring.
 */
void touch_handle_update(TouchState touch_state, int16_t x, int16_t y);

/**
 * @brief Pass a gesture to the service.
 *
 * Dropped while touch is globally disabled or an injected gesture
 * owns the sensor.
 *
 * @param gesture Detected gesture.
 * @param x X coordinate in pixels, before left-hand mode mirroring.
 * @param y Y coordinate in pixels, before left-hand mode mirroring.
 */
void touch_handle_gesture(TouchGesture gesture, int16_t x, int16_t y);

/**
 * @brief Reset the touch state.
 *
 * Forgets the finger state and last position and ends any injected gesture, without emitting
 * events.
 */
void touch_reset(void);

/**
 * @brief End an in-progress touch with a synthetic Liftoff.
 *
 * Uses the last known coordinates so backlight hold counters and gesture state unwind when touch
 * is torn down with a finger on the screen. Does nothing if no finger is down.
 */
void touch_release_active(void);

/** @brief Outcome of the session gate decision made on a Touchdown. */
typedef struct TouchWakeGateResult {
  /**
   * true when the touch must not drive navigation: the interaction session
   * (touch_session_is_active()) was inactive at Touchdown, i.e. unarmed contact on the idle
   * watchface.
   */
  bool latch;
} TouchWakeGateResult;

/**
 * @brief Stamp @c non_navigational onto a touch event.
 *
 * Latches the Touchdown decision across the whole gesture: @p gate is only consulted on a
 * Touchdown; PositionUpdate and Liftoff carry the latched value. KernelMain only.
 *
 * @param[in,out] event Touch event to stamp.
 * @param gate Gate decision for a Touchdown.
 */
void touch_wake_gate_stamp(TouchEvent *event, TouchWakeGateResult gate);

/**
 * @brief Set whether the display is rotated by 180 degrees (left-hand mode).
 *
 * When rotated, driver coordinates are mirrored to match the rotated framebuffer before being
 * dispatched.
 *
 * @param rotated true when rotated.
 */
void touch_set_rotated(bool rotated);

/**
 * @brief Phase of an injected gesture.
 *
 * Stated explicitly rather than inferred from the finger state: a mid-path sample and a fresh
 * touchdown are both "finger down", so a gesture that lost the sensor (a reset, or touch
 * switched off) could otherwise have its next sample taken as the start of a new one.
 */
typedef enum TouchInjectPhase {
  /** Touchdown, claiming the sensor. */
  TouchInjectPhase_Begin,
  /** Position update; requires the gesture to still own the sensor. */
  TouchInjectPhase_Move,
  /** Liftoff, releasing the sensor. */
  TouchInjectPhase_End,
} TouchInjectPhase;

/**
 * @brief Inject a synthetic touch sample.
 *
 * Intended for automated input (remote input endpoint, console commands), not for drivers.
 * Coordinates are the ones the UI observes: left-hand mode mirroring is not applied.
 *
 * A Begin arms the interaction session as a button press does, so contact on the idle watchface
 * is not dropped as unarmed. The sensor belongs to whoever puts a finger down first: a Begin is
 * refused while a physical finger is down, and physical samples are ignored until the injected
 * gesture ends. A Move or End is refused unless the gesture still owns the sensor, so the caller
 * learns it was interrupted.
 *
 * @code{.c}
 * // Swipe up through the middle of the screen.
 * if (touch_handle_injected_update(TouchInjectPhase_Begin, 100, 180)) {
 *   bool ok = touch_handle_injected_update(TouchInjectPhase_Move, 100, 120) &&
 *             touch_handle_injected_update(TouchInjectPhase_Move, 100, 60);
 *   if (ok) {
 *     touch_handle_injected_update(TouchInjectPhase_End, 100, 60);
 *   }
 * }
 * @endcode
 *
 * @param phase Gesture phase.
 * @param x X coordinate in pixels.
 * @param y Y coordinate in pixels.
 * @return false if the sample was dropped.
 */
bool touch_handle_injected_update(TouchInjectPhase phase, int16_t x, int16_t y);

/**
 * @brief Check whether a new injected gesture would be accepted.
 *
 * @return false if touch is globally disabled or a physical finger owns the sensor.
 */
bool touch_injection_is_available(void);

/** @} */
