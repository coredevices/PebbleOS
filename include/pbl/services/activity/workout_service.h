/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "activity.h"
#include "hr_util.h"

#include <stdbool.h>

#include <kernel/events.h>

/**
 * @defgroup services_activity_workout_service Workout service
 * @ingroup services_activity
 * @brief Manually started and stopped activity sessions.
 *
 * Workouts are like detected activity sessions, but are started and stopped by the user and
 * update more frequently. Only one workout runs at a time, and automatic session detection is
 * disabled while it runs. Workouts of at least a minute are saved as manual
 * @ref ActivitySession entries when stopped.
 *
 * @code{.c}
 * if (workout_service_start_workout(ActivitySessionType_Run)) {
 *   int32_t steps, duration_s, distance_m, bpm;
 *   HRZone zone;
 *
 *   workout_service_get_current_workout_info(&steps, &duration_s, &distance_m, &bpm, &zone);
 *   ...
 *   workout_service_stop_workout();
 * }
 * @endcode
 * @{
 */

/** @brief Initialize the workout service. */
void workout_service_init(void);

/**
 * @brief Signal that the workout app was opened.
 *
 * Cancels the abandoned workout reminder. Must be called from the app task.
 */
void workout_service_frontend_opened(void);

/**
 * @brief Signal that the workout app was closed.
 *
 * Arms a reminder notification for a workout left running. Must be called from the app task.
 */
void workout_service_frontend_closed(void);

/**
 * @brief Handle a health event (movement and heart rate updates).
 *
 * Called from the KernelMain event loop.
 *
 * @param event Event.
 */
void workout_service_health_event_handler(PebbleHealthEvent *event);

/**
 * @brief Handle an activity event.
 *
 * Pauses the workout when activity tracking stops. Called from the KernelMain event loop.
 *
 * @param event Event.
 */
void workout_service_activity_event_handler(PebbleActivityEvent *event);

/**
 * @brief Handle a workout event.
 *
 * Called from the KernelMain event loop.
 *
 * @param event Event.
 */
void workout_service_workout_event_handler(PebbleWorkoutEvent *event);

/**
 * @brief Check whether a workout is ongoing.
 *
 * @return true if a workout is ongoing, paused or not.
 */
bool workout_service_is_workout_ongoing(void);

/**
 * @brief Pause or resume the workout's heart rate sampling.
 *
 * Frees the shared optical path for a SpO2 reading during a workout. No-op without a workout
 * heart rate session.
 *
 * @param paused true to pause, false to resume.
 */
void workout_service_set_hrm_paused(bool paused);

/**
 * @brief Check whether a session type can be a workout.
 *
 * @param type Session type.
 * @return true for walks, runs and open workouts.
 */
bool workout_service_is_workout_type_supported(ActivitySessionType type);

/**
 * @brief Start a workout.
 *
 * Ends ongoing detected sessions and disables automatic session detection. Every started
 * workout must eventually be stopped.
 *
 * @param type Workout type.
 * @return true on success, false if the type is unsupported or a workout is already ongoing.
 */
bool workout_service_start_workout(ActivitySessionType type);

/**
 * @brief Pause or resume the current workout.
 *
 * Paused time does not count toward the workout duration.
 *
 * @param should_be_paused true to pause, false to resume.
 * @return true on success, false if no workout is ongoing.
 */
bool workout_service_pause_workout(bool should_be_paused);

/**
 * @brief Stop the current workout.
 *
 * Saves it as a session and notifies the user if it lasted at least a minute, and re-enables
 * automatic session detection.
 *
 * @return true on success, false if no workout is ongoing.
 */
bool workout_service_stop_workout(void);

/**
 * @brief Turn a detected activity session into a workout.
 *
 * The session is removed and a workout starts with its start time, duration, steps, distance and
 * calories.
 *
 * @param session Session to take over.
 * @return true on success.
 */
bool workout_service_takeover_activity_session(ActivitySession *session);

/**
 * @brief Check whether the current workout is paused.
 *
 * @return true if a workout is ongoing and paused.
 */
bool workout_service_is_paused(void);

/**
 * @brief Get the type of the current workout.
 *
 * @param[out] type_out Workout type.
 * @return true if a workout is ongoing.
 */
bool workout_service_get_current_workout_type(ActivitySessionType *type_out);

/**
 * @brief Get the current workout metrics.
 *
 * Each output is optional.
 *
 * @param[out] steps_out Steps.
 * @param[out] duration_s_out Duration excluding pauses, in seconds.
 * @param[out] distance_m_out Distance, in meters.
 * @param[out] current_bpm_out Current heart rate, in beats per minute.
 * @param[out] current_hr_zone_out Current heart rate zone.
 * @return true if a workout is ongoing.
 */
bool workout_service_get_current_workout_info(int32_t *steps_out, int32_t *duration_s_out,
                                              int32_t *distance_m_out, int32_t *current_bpm_out,
                                              HRZone *current_hr_zone_out);

/** @} */
