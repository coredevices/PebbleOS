/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <limits.h>
#include <string.h>

#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/services/blob_db/app_db.h>
#include <pbl/services/mic_capture.h>
#include <pbl/services/mic_manager.h>

#include <kernel/events.h>
#include <process_management/app_manager.h>
#include <process_management/pebble_process_md.h>

// Kernel heap, so the driver never writes into an app's heap. Freed once the mic is stopped.
typedef struct {
  int16_t driver_frame[MIC_CAPTURE_FRAME_SAMPLES];
  int16_t frames[2][MIC_CAPTURE_FRAME_SAMPLES];
} MicCaptureBuffers;

static PBL_MUTEX_DEFINE(s_lock);
static struct {
  bool active;
  //! mic_capture_service_start() is acquiring the mic unlocked; it frees the buffers on a stop.
  bool starting;
  bool event_pending;
  uint32_t session;
  Uuid app_uuid;
  MicCaptureBuffers *buffers;
  unsigned head;
  unsigned count;
} s_state;
static uint32_t s_next_session;
//! A start runs its checks unlocked; these let it tell that a revocation of its app or a focus
//! loss raced them.
static Uuid s_starting_uuid;
static uint32_t s_revocations;
static uint32_t s_focus_losses;

static void prv_post(uint8_t type, uint32_t session, MicCaptureStopReason reason) {
  PebbleEvent event = {
    .type = PEBBLE_MIC_CAPTURE_EVENT,
    .mic_capture = {.type = type, .session = session, .stop_reason = reason},
  };
  event_put(&event);
}

//! @return whether a capture was stopped. `buffers` is set for prv_release(), or to nullptr
//! while a start still owns them.
static bool prv_stop_locked(MicCaptureBuffers **buffers) {
  *buffers = nullptr;
  if (!s_state.active)
    return false;
  if (!s_state.starting)
    *buffers = s_state.buffers;
  s_state.active = false;
  s_state.starting = false;
  s_state.buffers = nullptr;
  s_state.count = 0;
  s_state.event_pending = false;
  return true;
}

//! Expects s_lock not held: the driver calls prv_data with its own lock held.
static void prv_release(MicCaptureBuffers *buffers) {
  mic_manager_release(MicClientAppCapture);
  pbl_free(buffers);
}

static void prv_stop_with_reason(MicCaptureStopReason reason) {
  MicCaptureBuffers *buffers;
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const uint32_t session = s_state.session;
  const bool stopped = prv_stop_locked(&buffers);
  pbl_mutex_unlock(&s_lock);
  if (!stopped)
    return;
  prv_release(buffers);
  prv_post(MicCaptureEventStopped, session, reason);
}

static void prv_preempted(void *unused) {
  prv_stop_with_reason(MicCaptureStopReasonInterrupted);
}

static void prv_data(int16_t *samples, size_t count, void *unused) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  bool post = false;
  const uint32_t session = s_state.session;
  if (s_state.active && count == MIC_CAPTURE_FRAME_SAMPLES && s_state.count < 2) {
    const unsigned index = (s_state.head + s_state.count) % 2;
    memcpy(s_state.buffers->frames[index], samples, sizeof(s_state.buffers->frames[index]));
    s_state.count++;
    post = !s_state.event_pending;
    s_state.event_pending = true;
  }
  pbl_mutex_unlock(&s_lock);
  if (!post)
    return;
  // Runs in the driver dispatch with its mutex held, which KernelMain may wait on in mic_stop()
  // while its queue is full: give up instead of resetting, and let the next frame retry.
  PebbleEvent event = {
    .type = PEBBLE_MIC_CAPTURE_EVENT,
    .mic_capture = {.type = MicCaptureEventData, .session = session},
  };
  if (!event_try_put_from_process(pebble_task_get_current(), &event)) {
    pbl_mutex_lock(&s_lock, PBL_FOREVER);
    if (s_state.session == session)
      s_state.event_pending = false;
    pbl_mutex_unlock(&s_lock);
  }
}

void mic_capture_service_init(void) {
  s_state.active = false;
  s_state.starting = false;
  s_state.count = 0;
  s_state.event_pending = false;
}

// Runs on the app task only, so starts never overlap.
int32_t mic_capture_service_start(PebbleTask task) {
  // Taken before the focus check, so a loss right after it is caught below
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const uint32_t focus_losses = s_focus_losses;
  pbl_mutex_unlock(&s_lock);
  switch (mic_manager_check_app(task)) {
    case MicManagerAppOk:
      break;
    case MicManagerAppWatchface:
      return -MicCaptureStartErrWatchface;
    case MicManagerAppNotForeground:
      return -MicCaptureStartErrNotForeground;
  }
  const PebbleProcessMd *md = app_manager_get_current_app_md();
  if (!md)
    return -MicCaptureStartErrNotForeground;

  // The flash lookup and the allocation run unlocked, so stops never wait on them
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_starting_uuid = md->uuid;
  const uint32_t revocations = s_revocations;
  const bool busy = s_state.active;
  pbl_mutex_unlock(&s_lock);
  if (busy)
    return -MicCaptureStartErrBusy;
  if (!app_db_microphone_granted(&md->uuid))
    return -MicCaptureStartErrPermissionDenied;
  MicCaptureBuffers *buffers = pbl_malloc(sizeof(*buffers));
  if (!buffers)
    return -MicCaptureStartErrNoMemory;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if ((s_revocations != revocations) || (s_focus_losses != focus_losses)) {
    const bool revoked = (s_revocations != revocations);
    pbl_mutex_unlock(&s_lock);
    pbl_free(buffers);
    return revoked ? -MicCaptureStartErrPermissionDenied : -MicCaptureStartErrNotForeground;
  }
  s_next_session = s_next_session >= INT32_MAX ? 1 : s_next_session + 1;
  const uint32_t session = s_next_session;
  s_state.session = session;
  s_state.app_uuid = md->uuid;
  s_state.head = 0;
  s_state.count = 0;
  s_state.event_pending = false;
  s_state.buffers = buffers;
  s_state.starting = true;
  s_state.active = true;
  pbl_mutex_unlock(&s_lock);

  const bool acquired =
      mic_manager_acquire(MicClientAppCapture, prv_data, nullptr, buffers->driver_frame,
                          MIC_CAPTURE_FRAME_SAMPLES, prv_preempted, nullptr);

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  // A stop meanwhile already told the app and left the buffers to us
  const bool ours = s_state.active && (s_state.session == session);
  s_state.starting = false;
  if (ours && !acquired) {
    s_state.active = false;
    s_state.buffers = nullptr;
  }
  pbl_mutex_unlock(&s_lock);

  if (ours && acquired)
    return (int32_t)session;
  prv_release(buffers);
  if (!ours)
    return (int32_t)session;
  return mic_manager_is_busy() ? -MicCaptureStartErrBusy : -MicCaptureStartErrUnavailable;
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
    memcpy(samples, s_state.buffers->frames[s_state.head], sizeof(s_state.buffers->frames[0]));
    s_state.head = (s_state.head + 1) % 2;
    s_state.count--;
    count = MIC_CAPTURE_FRAME_SAMPLES;
    s_state.event_pending = s_state.count != 0;
  }
  const bool post = count && s_state.event_pending;
  pbl_mutex_unlock(&s_lock);
  // Posted unlocked: event_put() can block, and the driver waits on s_lock with its lock held
  if (post)
    prv_post(MicCaptureEventData, session, 0);
  return count;
}

bool mic_capture_service_is_active(PebbleTask task, uint32_t session) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool active = prv_owned(task, session);
  pbl_mutex_unlock(&s_lock);
  return active;
}

void mic_capture_service_stop(PebbleTask task, uint32_t session) {
  MicCaptureBuffers *buffers;
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool stopped = prv_owned(task, session) && prv_stop_locked(&buffers);
  pbl_mutex_unlock(&s_lock);
  if (stopped)
    prv_release(buffers);
}

void mic_capture_service_stop_for_task(PebbleTask task) {
  if (task != PebbleTask_App)
    return;
  MicCaptureBuffers *buffers;
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool stopped = prv_stop_locked(&buffers);
  pbl_mutex_unlock(&s_lock);
  if (stopped)
    prv_release(buffers);
}

void mic_capture_service_handle_focus_lost(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_focus_losses++;
  pbl_mutex_unlock(&s_lock);
  prv_stop_with_reason(MicCaptureStopReasonFocusLost);
}

void mic_capture_service_handle_permission_changed(const Uuid *uuid, bool granted) {
  if (granted)
    return;
  MicCaptureBuffers *buffers;
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!uuid || uuid_equal(uuid, &s_starting_uuid))
    s_revocations++;
  const uint32_t session = s_state.session;
  const bool stopped = (!uuid || uuid_equal(uuid, &s_state.app_uuid)) && prv_stop_locked(&buffers);
  pbl_mutex_unlock(&s_lock);
  if (!stopped)
    return;
  prv_release(buffers);
  prv_post(MicCaptureEventStopped, session, MicCaptureStopReasonPermissionRevoked);
}
