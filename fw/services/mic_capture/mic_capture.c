/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <limits.h>
#include <string.h>

#include <pbl/kernel/mutex.h>
#include <pbl/services/blob_db/app_db.h>
#include <pbl/services/mic_capture.h>
#include <pbl/services/mic_manager.h>
#include <pbl/services/mic_stream.h>

#include <kernel/event_loop.h>
#include <kernel/events.h>
#include <kernel/ui/modals/modal_manager.h>
#include <popups/mic_banner.h>
#include <process_management/app_manager.h>
#include <process_management/pebble_process_md.h>

static PBL_MUTEX_DEFINE(s_lock);
static struct {
  bool active;
  bool event_pending;
  uint32_t session;
  Uuid app_uuid;
  unsigned head;
  unsigned count;
} s_state;
static uint32_t s_next_session;
// Driver storage outlives stop/preemption and never points into an app's heap.
static int16_t s_driver_frame[MIC_CAPTURE_FRAME_SAMPLES];
static int16_t s_frames[2][MIC_CAPTURE_FRAME_SAMPLES];

static void prv_post(uint8_t type, uint32_t session, MicCaptureStopReason reason) {
  PebbleEvent event = {
    .type = PEBBLE_MIC_CAPTURE_EVENT,
    .mic_capture = {.type = type, .session = session, .stop_reason = reason},
  };
  event_put(&event);
}

static void prv_banner(void *unused) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool active = s_state.active;
  pbl_mutex_unlock(&s_lock);
  if (active) {
    mic_banner_show();
  } else {
#ifdef CONFIG_SERVICE_MIC_STREAM
    if (mic_stream_service_is_active())
      return;
#endif
    mic_banner_hide();
  }
}

static bool prv_stop_locked(void) {
  if (!s_state.active)
    return false;
  s_state.active = false;
  s_state.count = 0;
  s_state.event_pending = false;
  return true;
}

static void prv_stop_with_reason(MicCaptureStopReason reason) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const uint32_t session = s_state.session;
  const bool stopped = prv_stop_locked();
  pbl_mutex_unlock(&s_lock);
  if (!stopped)
    return;
  mic_manager_release(MicClientAppCapture);
  launcher_task_add_callback(prv_banner, NULL);
  prv_post(MicCaptureEventStopped, session, reason);
}

static void prv_preempted(void *unused) {
  prv_stop_with_reason(MicCaptureStopReasonInterrupted);
}

static void prv_data(int16_t *samples, size_t count, void *unused) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_state.active && count == MIC_CAPTURE_FRAME_SAMPLES && s_state.count < 2) {
    const unsigned index = (s_state.head + s_state.count) % 2;
    memcpy(s_frames[index], samples, sizeof(s_frames[index]));
    s_state.count++;
    if (!s_state.event_pending) {
      s_state.event_pending = true;
      prv_post(MicCaptureEventData, s_state.session, 0);
    }
  }
  pbl_mutex_unlock(&s_lock);
}

void mic_capture_service_init(void) {
  s_state.active = false;
  s_state.count = 0;
  s_state.event_pending = false;
}

int32_t mic_capture_service_start(PebbleTask task) {
  if (task != PebbleTask_App)
    return -MicCaptureStartErrNotForeground;
  if (app_manager_is_watchface_running())
    return -MicCaptureStartErrWatchface;
  if (modal_manager_get_enabled() && !(modal_manager_get_properties() & ModalProperty_Unfocused)) {
    return -MicCaptureStartErrNotForeground;
  }
  const PebbleProcessMd *md = app_manager_get_current_app_md();
  if (!md)
    return -MicCaptureStartErrNotForeground;
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  int32_t result;
  if (s_state.active || mic_manager_get_owner() != MicClientNone) {
    result = -MicCaptureStartErrBusy;
  } else if (!app_db_microphone_granted(&md->uuid)) {
    result = -MicCaptureStartErrPermissionDenied;
  } else {
    s_next_session = s_next_session >= INT32_MAX ? 1 : s_next_session + 1;
    s_state.session = s_next_session;
    s_state.app_uuid = md->uuid;
    s_state.head = 0;
    s_state.count = 0;
    s_state.event_pending = false;
    s_state.active = true;
    if (mic_manager_acquire(MicClientAppCapture, prv_data, NULL, s_driver_frame,
                            MIC_CAPTURE_FRAME_SAMPLES, prv_preempted, NULL)) {
      result = (int32_t)s_state.session;
    } else {
      s_state.active = false;
      result = -MicCaptureStartErrUnavailable;
    }
  }
  pbl_mutex_unlock(&s_lock);
  if (result > 0)
    launcher_task_add_callback(prv_banner, NULL);
  return result;
}

static bool prv_owned(PebbleTask task, uint32_t session) {
  const PebbleProcessMd *md = app_manager_get_current_app_md();
  return task == PebbleTask_App && md && s_state.active && s_state.session == session &&
         uuid_equal(&md->uuid, &s_state.app_uuid);
}

size_t mic_capture_service_read(PebbleTask task, uint32_t session, int16_t *samples) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  size_t count = 0;
  if (samples && prv_owned(task, session) && s_state.count) {
    memcpy(samples, s_frames[s_state.head], sizeof(s_frames[0]));
    s_state.head = (s_state.head + 1) % 2;
    s_state.count--;
    count = MIC_CAPTURE_FRAME_SAMPLES;
    s_state.event_pending = s_state.count != 0;
    if (s_state.event_pending)
      prv_post(MicCaptureEventData, session, 0);
  }
  pbl_mutex_unlock(&s_lock);
  return count;
}

bool mic_capture_service_is_active(PebbleTask task, uint32_t session) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool active = prv_owned(task, session);
  pbl_mutex_unlock(&s_lock);
  return active;
}

void mic_capture_service_stop(PebbleTask task, uint32_t session) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool stopped = prv_owned(task, session) && prv_stop_locked();
  pbl_mutex_unlock(&s_lock);
  if (!stopped)
    return;
  mic_manager_release(MicClientAppCapture);
  launcher_task_add_callback(prv_banner, NULL);
}

void mic_capture_service_stop_for_task(PebbleTask task) {
  if (task != PebbleTask_App)
    return;
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool stopped = prv_stop_locked();
  pbl_mutex_unlock(&s_lock);
  if (!stopped)
    return;
  mic_manager_release(MicClientAppCapture);
  launcher_task_add_callback(prv_banner, NULL);
}

void mic_capture_service_handle_focus_lost(void) {
  prv_stop_with_reason(MicCaptureStopReasonFocusLost);
}

void mic_capture_service_handle_permission_changed(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const uint32_t session = s_state.session;
  const bool revoke = s_state.active && !app_db_microphone_granted(&s_state.app_uuid);
  if (revoke)
    prv_stop_locked();
  pbl_mutex_unlock(&s_lock);
  if (!revoke)
    return;
  mic_manager_release(MicClientAppCapture);
  launcher_task_add_callback(prv_banner, NULL);
  prv_post(MicCaptureEventStopped, session, MicCaptureStopReasonPermissionRevoked);
}
