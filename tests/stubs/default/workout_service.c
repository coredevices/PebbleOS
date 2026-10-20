/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/activity/activity.h>
#include <pbl/services/activity/hr_util.h>

#include <kernel/events.h>

PBL_WEAK void workout_service_init(void) {
}

PBL_WEAK void workout_service_frontend_opened(void) {
}

PBL_WEAK void workout_service_frontend_closed(void) {
}

PBL_WEAK void workout_service_health_event_handler(PebbleHealthEvent *event) {
}

PBL_WEAK bool workout_service_is_workout_ongoing(void) {
  return false;
}

PBL_WEAK bool workout_service_is_workout_type_supported(ActivitySessionType type) {
  return false;
}

PBL_WEAK bool workout_service_start_workout(ActivitySessionType type) {
  return false;
}

PBL_WEAK bool workout_service_pause_workout(bool should_be_paused) {
  return false;
}

PBL_WEAK bool workout_service_stop_workout(void) {
  return false;
}

PBL_WEAK bool workout_service_takeover_activity_session(ActivitySession *session) {
  return false;
}

PBL_WEAK bool workout_service_is_paused(void) {
  return false;
}

PBL_WEAK bool workout_service_get_current_workout_type(ActivitySessionType *type_out) {
  return false;
}

PBL_WEAK bool workout_service_get_current_workout_info(int32_t *steps_out, int32_t *duration_s_out,
                                                       int32_t *distance_m_out,
                                                       int32_t *current_bpm_out,
                                                       HRZone *current_hr_zone_out) {
  return false;
}

PBL_WEAK void workout_service_handle_spo2(uint8_t percent) {
}
