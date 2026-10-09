/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "alerts_private.h"

#include <stdint.h>

/**
 * @defgroup services_notifications_alerts_preferences Alert preferences
 * @ingroup services_notifications
 * @brief Persistent user preferences for alerts, Do Not Disturb and the speaker.
 *
 * Preferences live in the @c notifpref settings file. Setters update the cached value and write it
 * through to the file.
 * @{
 */

/** @brief "First use" dialogs whose completion is remembered. */
typedef enum FirstUseSource {
  /** Manual DND toggled from a notification action menu. */
  FirstUseSourceManualDNDActionMenu = 0,
  /** Manual DND toggled from the settings menu. */
  FirstUseSourceManualDNDSettingsMenu,
  /** Calendar-aware (smart) DND enabled. */
  FirstUseSourceSmartDND,
  /** Dismiss action used. */
  FirstUseSourceDismiss
} FirstUseSource;

/**
 * @brief Days of the week on which an app is muted.
 *
 * Bit @c n is set to mute on @c tm_wday @c n (bit 0 is Sunday, bit 6 Saturday).
 */
typedef enum MuteBitfield {
  /** Never muted. */
  MuteBitfield_None = 0b00000000,
  /** Muted every day. */
  MuteBitfield_Always = 0b01111111,
  /** Muted Monday to Friday. */
  MuteBitfield_Weekdays = 0b00111110,
  /** Muted Saturday and Sunday. */
  MuteBitfield_Weekends = 0b01000001,
} MuteBitfield;

/** @brief Whether notifications are displayed while Do Not Disturb is active. */
typedef enum {
  /** Notifications are not shown. */
  DndNotificationModeHide = 0,
  /** Notifications are shown (default). */
  DndNotificationModeShow = 1,
} DndNotificationMode;

/**
 * @brief Set whether notifications are displayed while DND is active.
 *
 * @param mode Display mode.
 */
void alerts_preferences_dnd_set_show_notifications(DndNotificationMode mode);

/**
 * @brief Get whether notifications are displayed while DND is active.
 *
 * @return Display mode.
 */
DndNotificationMode alerts_preferences_dnd_get_show_notifications(void);

/**
 * @brief Set whether motion turns on the backlight while DND is active.
 *
 * @param enable true to allow the motion backlight (default), false to suppress it.
 */
void alerts_preferences_dnd_set_motion_backlight(bool enable);

/**
 * @brief Get whether motion turns on the backlight while DND is active.
 *
 * @return true if the motion backlight is allowed.
 */
bool alerts_preferences_dnd_get_motion_backlight(void);

/**
 * @brief Set whether tap and double tap gestures turn on the backlight while DND is active.
 *
 * @param enable true to allow the touch backlight (default), false to suppress it.
 */
void alerts_preferences_dnd_set_touch_backlight(bool enable);

/**
 * @brief Get whether tap and double tap gestures turn on the backlight while DND is active.
 *
 * @return true if the touch backlight is allowed.
 */
bool alerts_preferences_dnd_get_touch_backlight(void);

/**
 * @brief Set whether the speaker is muted while DND is active.
 *
 * @param enable true to mute the speaker during DND, false to allow audio (default).
 */
void alerts_preferences_dnd_set_mute_speaker(bool enable);

/**
 * @brief Get whether the speaker is muted while DND is active.
 *
 * @return true if the speaker is muted during DND.
 */
bool alerts_preferences_dnd_get_mute_speaker(void);

/**
 * @brief Set whether notifications auto-dismiss while DND is active.
 *
 * @param enable true to allow auto-dismiss during DND, false to keep notifications on screen
 *               (default).
 */
void alerts_preferences_dnd_set_auto_dismiss(bool enable);

/**
 * @brief Get whether notifications auto-dismiss while DND is active.
 *
 * @return true if auto-dismiss is allowed during DND.
 */
bool alerts_preferences_dnd_get_auto_dismiss(void);

/**
 * @brief Set the always-on speaker mute, which silences the speaker regardless of DND.
 *
 * @param muted true to mute the speaker, false to allow audio (default).
 */
void alerts_preferences_set_speaker_muted(bool muted);

/**
 * @brief Get the always-on speaker mute.
 *
 * @return true if the speaker is muted.
 */
bool alerts_preferences_get_speaker_muted(void);

/**
 * @brief Set the system-wide speaker volume cap.
 *
 * Per-playback volumes are scaled by this value before being applied to the audio hardware.
 *
 * @param volume Volume cap, 0 to 100. Larger values are clamped to 100.
 */
void alerts_preferences_set_speaker_volume(uint8_t volume);

/**
 * @brief Get the system-wide speaker volume cap.
 *
 * @return Volume cap, 0 to 100. Defaults to 100.
 */
uint8_t alerts_preferences_get_speaker_volume(void);

/**
 * @brief Check whether a "first use" dialog has been shown, and mark it as shown.
 *
 * @param source Dialog to check.
 * @return true if the dialog had already been shown, false if this is the first time.
 */
bool alerts_preferences_check_and_set_first_use_complete(FirstUseSource source);

/** @} */
