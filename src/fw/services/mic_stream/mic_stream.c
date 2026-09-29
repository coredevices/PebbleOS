/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/mic_stream.h"

#include "kernel/event_loop.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "kernel/ui/modals/modal_manager.h"
#include "pbl/kernel/mutex.h"
#include "pbl/services/audio_endpoint.h"
#include "pbl/services/mic_manager.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/voice/voice_speex.h"
#include "pbl/services/voice_endpoint.h"
#include "popups/mic_banner.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include <pbl/logging/logging.h>

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
} MicStreamState;

static PBL_MUTEX_DEFINE(s_lock);
static MicStreamState s_state;
static TimerID s_setup_timer = TIMER_INVALID_ID;
static uint8_t s_packet[MAX_ENCODED_FRAME_SIZE];

// The banner is UI, so it is driven from KernelMain whichever task starts or stops the stream.
static void prv_show_banner_cb(void *unused) {
  mic_banner_show();
}

static void prv_hide_banner_cb(void *unused) {
  mic_banner_hide();
}

void mic_stream_service_init(void) {
  s_state = (MicStreamState){};
  s_setup_timer = new_timer_create();
}

static void prv_post_event(MicStreamEventType type, MicStreamServiceStopReason reason) {
  PebbleEvent e = {
    .type = PEBBLE_MIC_STREAM_EVENT,
    .mic_stream = {
      .type = type,
      .stop_reason = (uint8_t)reason,
    },
  };
  event_put(&e);
}

//! Expects s_lock held. Closes the phone session and frees resources.
static void prv_teardown_locked(bool phone_expects_stop) {
  new_timer_stop(s_setup_timer);
  if (s_state.transfer_open) {
    if (phone_expects_stop) {
      audio_endpoint_stop_transfer(s_state.session);
    } else {
      audio_endpoint_cancel_transfer(s_state.session);
    }
  }
  kernel_free(s_state.chunk);
  s_state = (MicStreamState){};
  launcher_task_add_callback(prv_hide_banner_cb, NULL);
}

//! Stops for a system-originated reason and tells the app.
static void prv_stop_with_reason(MicStreamServiceStopReason reason) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_state.active) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  PBL_LOG_DBG("Stream stopped, reason %u", reason);
  prv_teardown_locked(reason != MicStreamServiceStopReasonPhone);
  pbl_mutex_unlock(&s_lock);

  // No-op if the mic never started or dictation already took it
  mic_manager_release(MicClientAppCapture);
  prv_post_event(MicStreamEventStopped, reason);
}

// Runs on KernelBG, driven by the mic driver.
static void prv_mic_data_handler(int16_t *samples, size_t sample_count, void *context) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_state.active && !s_state.setup_pending && (sample_count == s_state.chunk_samples)) {
    const int len = voice_speex_encode_frame(samples, s_packet, sizeof(s_packet));
    if (len > 0) {
      audio_endpoint_add_frame(s_state.session, s_packet, (uint8_t)len);
    }
  }
  pbl_mutex_unlock(&s_lock);
}

static void prv_preempted(void *context) {
  prv_stop_with_reason(MicStreamServiceStopReasonPreempted);
}

static void prv_setup_timeout(void *data) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool pending = s_state.active && s_state.setup_pending;
  pbl_mutex_unlock(&s_lock);
  if (pending) {
    PBL_LOG_WRN("Phone did not answer the audio stream setup");
    prv_stop_with_reason(MicStreamServiceStopReasonPhone);
  }
}

static void prv_transfer_stopped(AudioEndpointSessionId session_id) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool ours = s_state.active && (s_state.session == session_id);
  if (ours) {
    // The endpoint already closed the session on its side
    s_state.transfer_open = false;
  }
  pbl_mutex_unlock(&s_lock);
  if (ours) {
    PBL_LOG_DBG("Phone stopped the audio stream");
    prv_stop_with_reason(MicStreamServiceStopReasonPhone);
  }
}

static bool prv_app_is_in_focus(void) {
  return !modal_manager_get_enabled() || (modal_manager_get_properties() & ModalProperty_Unfocused);
}

MicStreamServiceStartResult mic_stream_service_start(PebbleTask owner) {
  if (owner != PebbleTask_App) {
    return MicStreamServiceStartErrNotForeground;
  }
  if (app_manager_is_watchface_running()) {
    return MicStreamServiceStartErrWatchface;
  }
  if (!prv_app_is_in_focus()) {
    return MicStreamServiceStartErrNotForeground;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_state.active || (mic_manager_get_owner() != MicClientNone)) {
    pbl_mutex_unlock(&s_lock);
    return MicStreamServiceStartErrBusy;
  }
  // Shared with dictation, which stops the stream before it encodes
  if (!voice_speex_is_initialized() && !voice_speex_init()) {
    pbl_mutex_unlock(&s_lock);
    return MicStreamServiceStartErrNoMemory;
  }
  const uint16_t chunk_samples = voice_speex_get_frame_size();
  int16_t *chunk = kernel_malloc(chunk_samples * sizeof(int16_t));
  if (!chunk) {
    pbl_mutex_unlock(&s_lock);
    return MicStreamServiceStartErrNoMemory;
  }
  s_state = (MicStreamState){
    .active = true,
    .owner = owner,
    .setup_pending = true,
    .transfer_open = true,
    .chunk = chunk,
    .chunk_samples = chunk_samples,
  };
  s_state.session = audio_endpoint_setup_transfer(prv_transfer_stopped);

  AudioTransferInfoSpeex transfer_info;
  voice_speex_get_transfer_info(&transfer_info);

  // Third-party apps tag the session with their UUID so the phone can route it
  const PebbleProcessMd *md = app_manager_get_current_app_md();
  Uuid app_uuid = md ? md->uuid : UUID_INVALID;
  const bool from_app =
      md && !app_install_id_from_system(app_manager_get_current_app_id()) && md->is_unprivileged;
  voice_endpoint_setup_session(VoiceEndpointSessionTypeAudioStream, s_state.session, &transfer_info,
                               from_app ? &app_uuid : NULL);
  new_timer_start(s_setup_timer, SETUP_TIMEOUT_MS, prv_setup_timeout, NULL, 0);
  PBL_LOG_DBG("Audio stream requested, session %u", s_state.session);
  pbl_mutex_unlock(&s_lock);

  return MicStreamServiceStartOk;
}

void mic_stream_service_handle_setup_result(uint8_t voice_endpoint_result) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_state.active || !s_state.setup_pending) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  new_timer_stop(s_setup_timer);
  s_state.setup_pending = false;

  if (voice_endpoint_result != VoiceEndpointResultSuccess) {
    PBL_LOG_WRN("Phone refused the audio stream (%u)", voice_endpoint_result);
    prv_teardown_locked(false);
    pbl_mutex_unlock(&s_lock);
    prv_post_event(MicStreamEventStopped, MicStreamServiceStopReasonPhone);
    return;
  }

  if (!mic_manager_acquire(MicClientAppCapture, prv_mic_data_handler, NULL, s_state.chunk,
                           s_state.chunk_samples, prv_preempted, NULL)) {
    prv_teardown_locked(true);
    pbl_mutex_unlock(&s_lock);
    prv_post_event(MicStreamEventStopped, MicStreamServiceStopReasonError);
    return;
  }
  launcher_task_add_callback(prv_show_banner_cb, NULL);
  pbl_mutex_unlock(&s_lock);

  PBL_LOG_DBG("Audio stream started");
  prv_post_event(MicStreamEventStarted, MicStreamServiceStopReasonStopped);
}

static void prv_stop_silently(PebbleTask task) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_state.active || (s_state.owner != task)) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  prv_teardown_locked(true);
  pbl_mutex_unlock(&s_lock);
  mic_manager_release(MicClientAppCapture);
  PBL_LOG_DBG("Stream stopped");
}

void mic_stream_service_stop(PebbleTask owner) {
  prv_stop_silently(owner);
}

void mic_stream_service_stop_for_task(PebbleTask task) {
  prv_stop_silently(task);
}

bool mic_stream_service_is_active(void) {
  return s_state.active;
}

void mic_stream_service_handle_app_focus_lost(void) {
  prv_stop_with_reason(MicStreamServiceStopReasonFocusLost);
}

void mic_stream_service_handle_system_preempt(void) {
  prv_stop_with_reason(MicStreamServiceStopReasonPreempted);
}
