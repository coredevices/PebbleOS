/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/mic_capture/mic_capture_service.h"

#include "kernel/event_loop.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "kernel/ui/modals/modal_manager.h"
#include "pbl/kernel/mutex.h"
#include "pbl/services/app_permissions/app_permissions.h"
#include "pbl/services/audio_encoder/audio_encoder.h"
#include "pbl/services/audio_endpoint.h"
#include "pbl/services/mic_manager.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/voice_endpoint.h"
#include "pbl/util/circular_buffer.h"
#include "popups/mic_banner.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DEFINE(service_mic_capture, CONFIG_SERVICE_MIC_CAPTURE_LOG_LEVEL);

#define RING_BYTES              (MIC_CAPTURE_RING_SAMPLES * sizeof(int16_t))
#define STREAM_SETUP_TIMEOUT_MS (8000)

typedef enum {
  MicCaptureSinkApp = 0, //!< PCM batches to the app
  MicCaptureSinkPhone,   //!< Encoded frames to the phone
} MicCaptureSink;

typedef struct {
  bool active;
  PebbleTask owner;
  MicCaptureSink sink;
  uint16_t samples_per_update;
  int16_t *chunk; //!< The mic driver fills this, samples_per_update samples

  // App sink
  CircularBuffer ring;
  uint8_t *ring_storage;
  bool overrun;
  bool data_event_pending;

  // Phone sink
  bool stream_setup_pending; //!< Waiting for the phone to accept the session
  bool stream_transfer_open; //!< audio_endpoint transfer set up
  AudioEndpointSessionId stream_session;
  AudioEncoderInfo encoder;
  uint8_t *packet; //!< encoder.max_packet_bytes
} MicCaptureState;

static PBL_MUTEX_DEFINE(s_lock);
static MicCaptureState s_state;
static TimerID s_setup_timer = TIMER_INVALID_ID;

// The banner is UI, so it is driven from KernelMain whichever task stops or starts capture.
static void prv_show_banner_cb(void *unused) {
  mic_banner_show();
}

static void prv_hide_banner_cb(void *unused) {
  mic_banner_hide();
}

void mic_capture_service_init(void) {
  s_state = (MicCaptureState){};
  s_setup_timer = new_timer_create();
}

static void prv_post_event(uint8_t type, MicCaptureStopReason reason, bool overrun,
                           uint16_t num_samples) {
  PebbleEvent e = {
    .type = PEBBLE_MIC_CAPTURE_EVENT,
    .mic_capture = {
      .type = type,
      .stop_reason = (uint8_t)reason,
      .overrun = overrun,
      .num_samples = num_samples,
    },
  };
  event_put(&e);
}

//! Expects s_lock held. Frees resources and closes the phone session, if any.
static void prv_teardown_locked(bool phone_still_expects_stop) {
  if (s_state.sink == MicCaptureSinkPhone) {
    new_timer_stop(s_setup_timer);
    if (s_state.stream_transfer_open) {
      if (phone_still_expects_stop) {
        audio_endpoint_stop_transfer(s_state.stream_session);
      } else {
        audio_endpoint_cancel_transfer(s_state.stream_session);
      }
    }
    audio_encoder_service_close(s_state.owner);
  }
  kernel_free(s_state.ring_storage);
  kernel_free(s_state.chunk);
  kernel_free(s_state.packet);
  s_state = (MicCaptureState){};
  launcher_task_add_callback(prv_hide_banner_cb, NULL);
}

//! Stops capture for a system-originated reason and tells the app. `release_mic` is false when
//! the mic manager already took the mic away (preemption).
static void prv_stop_with_reason(MicCaptureStopReason reason, bool release_mic) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_state.active) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  PBL_LOG_DBG("Capture stopped, reason %u", reason);
  prv_teardown_locked(reason != MicCaptureStopReasonPhone);
  pbl_mutex_unlock(&s_lock);

  if (release_mic) {
    mic_manager_release(MicClientAppCapture);
  }
  prv_post_event(MicCaptureEventStopped, reason, false, 0);
}

// Runs on KernelBG, driven by the mic driver.
static void prv_mic_data_handler(int16_t *samples, size_t sample_count, void *context) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_state.active) {
    pbl_mutex_unlock(&s_lock);
    return;
  }

  if (s_state.sink == MicCaptureSinkPhone) {
    const int len = audio_encoder_service_encode(s_state.owner, samples, sample_count,
                                                 s_state.packet, s_state.encoder.max_packet_bytes);
    if (len > 0) {
      audio_endpoint_add_frame(s_state.stream_session, s_state.packet, (uint8_t)len);
    }
    pbl_mutex_unlock(&s_lock);
    return;
  }

  const uint16_t bytes = sample_count * sizeof(int16_t);
  if (!circular_buffer_write(&s_state.ring, samples, bytes)) {
    // Full: drop the newest chunk, the app is not keeping up
    s_state.overrun = true;
  }
  bool post = false;
  bool overrun = false;
  uint16_t available = 0;
  if (!s_state.data_event_pending) {
    s_state.data_event_pending = true;
    post = true;
    overrun = s_state.overrun;
    s_state.overrun = false;
    available = circular_buffer_get_read_space_remaining(&s_state.ring) / sizeof(int16_t);
  }
  pbl_mutex_unlock(&s_lock);

  if (post) {
    prv_post_event(MicCaptureEventData, MicCaptureStopReasonStopped, overrun, available);
  }
}

static void prv_preempted(void *context) {
  prv_stop_with_reason(MicCaptureStopReasonPreempted, false /* mic already gone */);
}

static bool prv_app_is_in_focus(void) {
  return !modal_manager_get_enabled() || (modal_manager_get_properties() & ModalProperty_Unfocused);
}

//! Common admission checks. Expects s_lock NOT held.
static MicCaptureStartResult prv_check_start(PebbleTask owner) {
  if (owner != PebbleTask_App) {
    return MicCaptureStartErrNotForeground;
  }
  if (app_manager_is_watchface_running()) {
    return MicCaptureStartErrWatchface;
  }
  if (!prv_app_is_in_focus()) {
    return MicCaptureStartErrNotForeground;
  }
  switch (app_permissions_get_state_for_current_app(AppPermission_Microphone)) {
    case AppPermissionStateNotDeclared:
      return MicCaptureStartErrNotDeclared;
    case AppPermissionStateDenied:
      return MicCaptureStartErrDenied;
    case AppPermissionStateGranted:
      break;
  }
  return MicCaptureStartOk;
}

//! Expects s_lock held and s_state prepared. Starts the mic and shows the banner.
static bool prv_acquire_mic_locked(void) {
  if (!mic_manager_acquire(MicClientAppCapture, prv_mic_data_handler, NULL, s_state.chunk,
                           s_state.samples_per_update, prv_preempted, NULL)) {
    return false;
  }
  launcher_task_add_callback(prv_show_banner_cb, NULL);
  return true;
}

MicCaptureStartResult mic_capture_service_start(PebbleTask owner, uint16_t samples_per_update) {
  if ((samples_per_update < MIC_CAPTURE_MIN_SAMPLES_PER_UPDATE) ||
      (samples_per_update > MIC_CAPTURE_MAX_SAMPLES_PER_UPDATE)) {
    return MicCaptureStartErrInvalidArgs;
  }
  const MicCaptureStartResult check = prv_check_start(owner);
  if (check != MicCaptureStartOk) {
    return check;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_state.active) {
    pbl_mutex_unlock(&s_lock);
    return MicCaptureStartErrBusy;
  }

  uint8_t *ring_storage = kernel_malloc(RING_BYTES);
  int16_t *chunk = kernel_malloc(samples_per_update * sizeof(int16_t));
  if (!ring_storage || !chunk) {
    kernel_free(ring_storage);
    kernel_free(chunk);
    pbl_mutex_unlock(&s_lock);
    return MicCaptureStartErrNoMemory;
  }

  s_state = (MicCaptureState){
    .active = true,
    .owner = owner,
    .sink = MicCaptureSinkApp,
    .samples_per_update = samples_per_update,
    .ring_storage = ring_storage,
    .chunk = chunk,
  };
  circular_buffer_init(&s_state.ring, ring_storage, RING_BYTES);

  if (!prv_acquire_mic_locked()) {
    prv_teardown_locked(false);
    pbl_mutex_unlock(&s_lock);
    return MicCaptureStartErrBusy;
  }
  pbl_mutex_unlock(&s_lock);

  PBL_LOG_DBG("Capture started, %u samples per update", samples_per_update);
  return MicCaptureStartOk;
}

// Stream to phone
////////////////////////////////////////////////////////////////////////////////

static void prv_stream_setup_timeout(void *data) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool pending = s_state.active && s_state.stream_setup_pending;
  pbl_mutex_unlock(&s_lock);
  if (pending) {
    PBL_LOG_WRN("Phone did not answer the audio stream setup");
    prv_stop_with_reason(MicCaptureStopReasonPhone, false /* mic never started */);
  }
}

static void prv_stream_transfer_stopped(AudioEndpointSessionId session_id) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool ours = s_state.active && (s_state.sink == MicCaptureSinkPhone) &&
                    (s_state.stream_session == session_id);
  if (ours) {
    // The endpoint already closed the session on its side
    s_state.stream_transfer_open = false;
  }
  const bool mic_running = ours && !s_state.stream_setup_pending;
  pbl_mutex_unlock(&s_lock);
  if (ours) {
    PBL_LOG_DBG("Phone stopped the audio stream");
    prv_stop_with_reason(MicCaptureStopReasonPhone, mic_running);
  }
}

MicCaptureStartResult mic_capture_service_start_stream(PebbleTask owner) {
  const MicCaptureStartResult check = prv_check_start(owner);
  if (check != MicCaptureStartOk) {
    return check;
  }
  if (!audio_encoder_service_is_codec_available(AudioCodecSpeexWB)) {
    return MicCaptureStartErrBusy;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_state.active || (mic_manager_get_owner() != MicClientNone)) {
    pbl_mutex_unlock(&s_lock);
    return MicCaptureStartErrBusy;
  }

  AudioEncoderInfo info;
  if (!audio_encoder_service_open(AudioCodecSpeexWB, owner, &info)) {
    pbl_mutex_unlock(&s_lock);
    return MicCaptureStartErrBusy;
  }
  const uint16_t samples_per_update = info.frame_samples * info.channels;
  int16_t *chunk = kernel_malloc(samples_per_update * sizeof(int16_t));
  uint8_t *packet = kernel_malloc(info.max_packet_bytes);
  if (!chunk || !packet) {
    kernel_free(chunk);
    kernel_free(packet);
    audio_encoder_service_close(owner);
    pbl_mutex_unlock(&s_lock);
    return MicCaptureStartErrNoMemory;
  }
  s_state = (MicCaptureState){
    .active = true,
    .owner = owner,
    .sink = MicCaptureSinkPhone,
    .samples_per_update = samples_per_update,
    .chunk = chunk,
    .packet = packet,
    .encoder = info,
    .stream_setup_pending = true,
  };
  s_state.stream_session = audio_endpoint_setup_transfer(prv_stream_transfer_stopped);
  s_state.stream_transfer_open = true;

  AudioTransferInfoSpeex transfer_info = {
    .sample_rate = info.sample_rate,
    .bit_rate = (uint16_t)info.bitrate,
    .frame_size = info.frame_samples,
    .bitstream_version = info.bitstream_version,
  };
  strncpy(transfer_info.version, "1.2.1", sizeof(transfer_info.version) - 1);

  const PebbleProcessMd *md = app_manager_get_current_app_md();
  Uuid app_uuid = md ? md->uuid : UUID_INVALID;
  const bool from_app =
      md && !app_install_id_from_system(app_manager_get_current_app_id()) && md->is_unprivileged;
  voice_endpoint_setup_session(VoiceEndpointSessionTypeAudioStream, s_state.stream_session,
                               &transfer_info, from_app ? &app_uuid : NULL);
  new_timer_start(s_setup_timer, STREAM_SETUP_TIMEOUT_MS, prv_stream_setup_timeout, NULL, 0);
  pbl_mutex_unlock(&s_lock);

  PBL_LOG_DBG("Audio stream requested, session %u", s_state.stream_session);
  return MicCaptureStartOk;
}

void mic_capture_service_handle_stream_setup_result(uint8_t voice_endpoint_result) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_state.active || (s_state.sink != MicCaptureSinkPhone) || !s_state.stream_setup_pending) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  new_timer_stop(s_setup_timer);
  s_state.stream_setup_pending = false;

  if (voice_endpoint_result != VoiceEndpointResultSuccess) {
    PBL_LOG_WRN("Phone refused the audio stream (%u)", voice_endpoint_result);
    prv_teardown_locked(false);
    pbl_mutex_unlock(&s_lock);
    prv_post_event(MicCaptureEventStopped, MicCaptureStopReasonPhone, false, 0);
    return;
  }

  if (!prv_acquire_mic_locked()) {
    prv_teardown_locked(true);
    pbl_mutex_unlock(&s_lock);
    prv_post_event(MicCaptureEventStopped, MicCaptureStopReasonError, false, 0);
    return;
  }
  pbl_mutex_unlock(&s_lock);

  PBL_LOG_DBG("Audio stream started");
  prv_post_event(MicCaptureEventStarted, MicCaptureStopReasonStopped, false, 0);
}

// Stopping
////////////////////////////////////////////////////////////////////////////////

static void prv_stop_silently(PebbleTask task) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_state.active || (s_state.owner != task)) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  prv_teardown_locked(true);
  pbl_mutex_unlock(&s_lock);
  mic_manager_release(MicClientAppCapture);
  PBL_LOG_DBG("Capture stopped");
}

void mic_capture_service_stop(PebbleTask owner) {
  prv_stop_silently(owner);
}

void mic_capture_service_stop_for_task(PebbleTask task) {
  prv_stop_silently(task);
}

uint32_t mic_capture_service_read(PebbleTask owner, int16_t *out, uint32_t max_samples) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_state.active || (s_state.owner != owner) || (s_state.sink != MicCaptureSinkApp) || !out) {
    pbl_mutex_unlock(&s_lock);
    return 0;
  }
  s_state.data_event_pending = false;
  const uint32_t available =
      circular_buffer_get_read_space_remaining(&s_state.ring) / sizeof(int16_t);
  const uint32_t num_samples = (max_samples < available) ? max_samples : available;
  if (num_samples > 0) {
    const uint16_t bytes = num_samples * sizeof(int16_t);
    circular_buffer_copy(&s_state.ring, out, bytes);
    circular_buffer_consume(&s_state.ring, bytes);
  }
  pbl_mutex_unlock(&s_lock);
  return num_samples;
}

uint32_t mic_capture_service_get_available(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const uint32_t available =
      (s_state.active && (s_state.sink == MicCaptureSinkApp))
          ? circular_buffer_get_read_space_remaining(&s_state.ring) / sizeof(int16_t)
          : 0;
  pbl_mutex_unlock(&s_lock);
  return available;
}

bool mic_capture_service_is_active(void) {
  return s_state.active;
}

void mic_capture_service_handle_app_focus_lost(void) {
  prv_stop_with_reason(MicCaptureStopReasonFocusLost, true);
}

void mic_capture_service_handle_permission_changed(void) {
  if (!s_state.active) {
    return;
  }
  if (!app_permissions_is_granted_for_current_app(AppPermission_Microphone)) {
    prv_stop_with_reason(MicCaptureStopReasonPermissionRevoked, true);
  }
}

void mic_capture_service_handle_system_preempt(void) {
  prv_stop_with_reason(MicCaptureStopReasonPreempted, true);
}
