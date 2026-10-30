/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/services/audio_endpoint.h>
#include <pbl/services/comm_session/session.h>
#include <pbl/services/mic_manager.h>
#include <pbl/services/mic_stream.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/services/system_task.h>
#include <pbl/services/voice/voice_speex.h>
#include <pbl/services/voice_endpoint.h>

#include <kernel/events.h>
#include <kernel/pbl_malloc.h>
#include <process_management/app_install_manager.h>
#include <process_management/app_manager.h>

PBL_LOG_MODULE_DEFINE(service_mic_stream, CONFIG_SERVICE_MIC_STREAM_LOG_LEVEL);

#define SETUP_TIMEOUT_MS       (8000)
#define MAX_ENCODED_FRAME_SIZE (200)

typedef struct {
  bool active;
  PebbleTask owner;
  bool setup_pending; //!< Waiting for the phone to accept the session
  bool transfer_open; //!< audio_endpoint transfer set up
  AudioEndpointSessionId session;
  int16_t *chunk; //!< The mic driver fills this, one Speex frame
  uint16_t chunk_samples;
  uint32_t stream_id; //!< Tags events so the app can drop ones from an earlier stream
} MicStreamState;

//! What a teardown leaves for prv_finish_teardown(), once s_lock is released.
typedef struct {
  int16_t *chunk;
  AudioEndpointSessionId session;
  bool close_transfer;
  bool phone_expects_stop;
} MicStreamTeardown;

static PBL_MUTEX_DEFINE(s_lock);
static MicStreamState s_state;
static uint32_t s_last_stream_id;
static TimerID s_setup_timer = TIMER_INVALID_ID;
static uint8_t s_packet[MAX_ENCODED_FRAME_SIZE];
//! Lets a start, which runs its checks unlocked, tell that a focus loss raced them
static uint32_t s_focus_losses;

void mic_stream_service_init(void) {
  s_state = (MicStreamState){};
  s_setup_timer = new_timer_create();
}

static void prv_post_event(uint32_t stream_id, MicStreamEventType type,
                           MicStreamServiceStopReason reason) {
  PebbleEvent e = {
    .type = PEBBLE_MIC_STREAM_EVENT,
    .mic_stream = {
      .type = type,
      .stop_reason = (uint8_t)reason,
      .stream_id = stream_id,
    },
  };
  event_put(&e);
}

//! Expects s_lock held. Resets the state; the rest is left to prv_finish_teardown().
static MicStreamTeardown prv_teardown_locked(bool phone_expects_stop) {
  new_timer_stop(s_setup_timer);
  const MicStreamTeardown teardown = {
    .chunk = s_state.chunk,
    .session = s_state.session,
    .close_transfer = s_state.transfer_open,
    .phone_expects_stop = phone_expects_stop,
  };
  s_state = (MicStreamState){};
  return teardown;
}

//! Expects s_lock not held: the driver calls prv_mic_data_handler with its own lock held, and the
//! endpoint takes bt_lock and may block on the send.
static void prv_finish_teardown(const MicStreamTeardown *teardown) {
  // The mic stops first, so no frame arrives while the stop is sent. Also drops the reservation;
  // no-op on the mic if it never started or dictation already took it.
  mic_manager_release(MicClientAppStream);
  voice_speex_deinit();
  kernel_free(teardown->chunk);
  if (teardown->close_transfer) {
    if (teardown->phone_expects_stop) {
      audio_endpoint_stop_transfer(teardown->session);
    } else {
      audio_endpoint_cancel_transfer(teardown->session);
    }
  }
}

//! Stops for a system-originated reason and tells the app. A stream_id of 0 matches any stream.
static void prv_stop_with_reason(uint32_t stream_id, MicStreamServiceStopReason reason) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_state.active || (stream_id && (stream_id != s_state.stream_id))) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  PBL_LOG_DBG("Stream stopped, reason %u", reason);
  stream_id = s_state.stream_id;
  const MicStreamTeardown teardown = prv_teardown_locked(reason != MicStreamServiceStopReasonPhone);
  pbl_mutex_unlock(&s_lock);

  prv_finish_teardown(&teardown);
  prv_post_event(stream_id, MicStreamEventStopped, reason);
}

// Runs on KernelBG, driven by the mic driver.
static void prv_mic_data_handler(int16_t *samples, size_t sample_count, void *context) {
  int len = 0;
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const AudioEndpointSessionId session = s_state.session;
  if (s_state.active && !s_state.setup_pending && (sample_count == s_state.chunk_samples)) {
    len = voice_speex_encode_frame(samples, s_packet, sizeof(s_packet));
  }
  pbl_mutex_unlock(&s_lock);
  // Unlocked as it takes bt_lock. A frame for a session that just closed is dropped.
  if (len > 0) {
    audio_endpoint_add_frame(session, s_packet, (uint8_t)len);
  }
}

static void prv_preempted(void *context) {
  prv_stop_with_reason(0, MicStreamServiceStopReasonPreempted);
}

static void prv_setup_timeout_bg(void *data) {
  const uint32_t stream_id = (uintptr_t)data;
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool pending = s_state.active && s_state.setup_pending && (s_state.stream_id == stream_id);
  pbl_mutex_unlock(&s_lock);
  if (pending) {
    PBL_LOG_WRN("Phone did not answer the audio stream setup");
    prv_stop_with_reason(stream_id, MicStreamServiceStopReasonPhone);
  }
}

//! The teardown takes bt_lock and stops the mic, so it never runs on the timer task.
static void prv_setup_timeout(void *data) {
  system_task_add_callback(prv_setup_timeout_bg, data);
}

static void prv_transfer_stopped_bg(void *data) {
  const AudioEndpointSessionId session_id = (uintptr_t)data;
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool ours = s_state.active && (s_state.session == session_id);
  const uint32_t stream_id = s_state.stream_id;
  if (ours) {
    // The endpoint already closed the session on its side
    s_state.transfer_open = false;
  }
  pbl_mutex_unlock(&s_lock);
  if (ours) {
    PBL_LOG_DBG("Phone stopped the audio stream");
    prv_stop_with_reason(stream_id, MicStreamServiceStopReasonPhone);
  }
}

//! Called by the endpoint with bt_lock held, so s_lock is only taken on KernelBG.
static void prv_transfer_stopped(AudioEndpointSessionId session_id) {
  system_task_add_callback(prv_transfer_stopped_bg, (void *)(uintptr_t)session_id);
}

MicStreamServiceStartResult mic_stream_service_start(PebbleTask owner, uint32_t *stream_id) {
  // Taken before the focus check, so a loss right after it is caught below
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const uint32_t focus_losses = s_focus_losses;
  pbl_mutex_unlock(&s_lock);
  switch (mic_manager_check_app(owner)) {
    case MicManagerAppOk:
      break;
    case MicManagerAppWatchface:
      return MicStreamServiceStartErrWatchface;
    case MicManagerAppNotForeground:
      return MicStreamServiceStartErrNotForeground;
  }
  if (!comm_session_get_system_session()) {
    return MicStreamServiceStartErrUnavailable;
  }

  // Held through the phone handshake so no other app takes the mic meanwhile. Only one start can
  // hold it, so the rest runs unlocked until the state is published.
  if (!mic_manager_reserve(MicClientAppStream)) {
    return MicStreamServiceStartErrBusy;
  }
  // Shared with dictation, which can't start while the mic is reserved. Released on teardown.
  if (!voice_speex_init()) {
    mic_manager_release(MicClientAppStream);
    return MicStreamServiceStartErrNoMemory;
  }
  const uint16_t chunk_samples = voice_speex_get_frame_size();
  int16_t *chunk = kernel_malloc(chunk_samples * sizeof(int16_t));
  if (!chunk) {
    mic_manager_release(MicClientAppStream);
    voice_speex_deinit();
    return MicStreamServiceStartErrNoMemory;
  }
  // Fails while a previous session still closes
  const AudioEndpointSessionId session = audio_endpoint_setup_transfer(prv_transfer_stopped);
  if (session == AUDIO_ENDPOINT_SESSION_INVALID_ID) {
    kernel_free(chunk);
    mic_manager_release(MicClientAppStream);
    voice_speex_deinit();
    return MicStreamServiceStartErrBusy;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_focus_losses != focus_losses) {
    pbl_mutex_unlock(&s_lock);
    kernel_free(chunk);
    mic_manager_release(MicClientAppStream);
    voice_speex_deinit();
    audio_endpoint_cancel_transfer(session);
    return MicStreamServiceStartErrNotForeground;
  }
  s_last_stream_id = (s_last_stream_id == UINT32_MAX) ? 1 : (s_last_stream_id + 1);
  s_state = (MicStreamState){
    .active = true,
    .owner = owner,
    .setup_pending = true,
    .transfer_open = true,
    .chunk = chunk,
    .chunk_samples = chunk_samples,
    .session = session,
    .stream_id = s_last_stream_id,
  };
  const uint32_t id = s_state.stream_id;
  new_timer_start(s_setup_timer, SETUP_TIMEOUT_MS, prv_setup_timeout, (void *)(uintptr_t)id, 0);
  pbl_mutex_unlock(&s_lock);

  AudioTransferInfoSpeex transfer_info;
  voice_speex_get_transfer_info(&transfer_info);

  // Third-party apps tag the session with their UUID so the phone can route it
  const PebbleProcessMd *md = app_manager_get_current_app_md();
  Uuid app_uuid = md ? md->uuid : UUID_INVALID;
  const bool from_app =
      md && !app_install_id_from_system(app_manager_get_current_app_id()) && md->is_unprivileged;
  // Sent unlocked, as it may block. A stop meanwhile leaves the answer unmatched.
  voice_endpoint_setup_session(VoiceEndpointSessionTypeAudioStream, session, &transfer_info,
                               from_app ? &app_uuid : nullptr);
  PBL_LOG_DBG("Audio stream requested, session %u", session);
  if (stream_id) {
    *stream_id = id;
  }
  return MicStreamServiceStartOk;
}

void mic_stream_service_handle_setup_result(AudioEndpointSessionId session_id,
                                            uint8_t voice_endpoint_result) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_state.active || !s_state.setup_pending || (s_state.session != session_id)) {
    pbl_mutex_unlock(&s_lock);
    PBL_LOG_DBG("Ignoring setup result for session %u", session_id);
    return;
  }
  new_timer_stop(s_setup_timer);
  s_state.setup_pending = false;
  const uint32_t stream_id = s_state.stream_id;

  if (voice_endpoint_result != VoiceEndpointResultSuccess) {
    PBL_LOG_WRN("Phone refused the audio stream (%u)", voice_endpoint_result);
    const MicStreamTeardown teardown = prv_teardown_locked(false);
    pbl_mutex_unlock(&s_lock);
    prv_finish_teardown(&teardown);
    prv_post_event(stream_id, MicStreamEventStopped, MicStreamServiceStopReasonPhone);
    return;
  }

  if (!mic_manager_acquire(MicClientAppStream, prv_mic_data_handler, nullptr, s_state.chunk,
                           s_state.chunk_samples, prv_preempted, nullptr)) {
    const MicStreamTeardown teardown = prv_teardown_locked(true);
    pbl_mutex_unlock(&s_lock);
    prv_finish_teardown(&teardown);
    prv_post_event(stream_id, MicStreamEventStopped, MicStreamServiceStopReasonError);
    return;
  }
  pbl_mutex_unlock(&s_lock);

  PBL_LOG_DBG("Audio stream started");
  prv_post_event(stream_id, MicStreamEventStarted, MicStreamServiceStopReasonStopped);
}

void mic_stream_service_stop(PebbleTask task) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_state.active || (s_state.owner != task)) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  const MicStreamTeardown teardown = prv_teardown_locked(true);
  pbl_mutex_unlock(&s_lock);
  prv_finish_teardown(&teardown);
  PBL_LOG_DBG("Stream stopped");
}

bool mic_stream_service_is_active(void) {
  return s_state.active;
}

void mic_stream_service_handle_app_focus_lost(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_focus_losses++;
  pbl_mutex_unlock(&s_lock);
  prv_stop_with_reason(0, MicStreamServiceStopReasonFocusLost);
}

void mic_stream_service_handle_system_preempt(void) {
  prv_stop_with_reason(0, MicStreamServiceStopReasonPreempted);
}

void mic_stream_service_handle_comm_session_event(const PebbleCommSessionEvent *event) {
  if (event->is_system && !event->is_open) {
    // The phone can't answer or send a stop any more
    prv_stop_with_reason(0, MicStreamServiceStopReasonPhone);
  }
}
