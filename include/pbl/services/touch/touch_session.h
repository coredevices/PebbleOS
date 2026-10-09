/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_touch_touch_session Touch interaction session
 * @ingroup services_touch
 * @brief Interaction session gating raw touch navigation.
 *
 * Raw touchdowns only navigate (and hold the backlight) while the session is active. Deliberate
 * interaction arms it: the backlight wake gesture, a button press or injected input. Use extends
 * it; it expires after the backlight timeout, as if the light had come on, tracked as its own
 * deadline because bright ambient light (or DnD) can keep the backlight off. The idle watchface
 * is the only guarded surface: any other foreground UI, a focused modal, or a backlight lit by
 * anything other than touch counts as active. Only contact that moved or formed a gesture
 * extends the session.
 *
 * All functions run on KernelMain; the module has no locking.
 * @{
 */

/** @brief What armed the session. */
typedef enum TouchSessionArmSource {
  /** Backlight wake gesture. */
  TouchSessionArmSource_WakeGesture,
  /** Button press. */
  TouchSessionArmSource_Button,
  /** Synthetic touch injected for automated input; deliberate by construction. */
  TouchSessionArmSource_Injected,
} TouchSessionArmSource;

/**
 * @brief Open or re-open the session after deliberate interaction.
 *
 * @param source What armed the session.
 */
void touch_session_arm(TouchSessionArmSource source);

/**
 * @brief Push the session deadline out, only while the session is active.
 *
 * Gated contact must not keep itself alive.
 */
void touch_session_extend(void);

/**
 * @brief Check whether touch may navigate.
 *
 * @return true within the armed deadline, when the foreground UI is not a watchface, a focused
 * modal is up, or the backlight is on for a reason other than touch. Always true in recovery
 * firmware.
 */
bool touch_session_is_active(void);

/**
 * @brief Expire the session deadline immediately.
 *
 * The overrides in touch_session_is_active() still apply.
 */
void touch_session_reset(void);

/** @} */
