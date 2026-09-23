/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "kernel/events.h"
#include "kernel/ui/modals/modal_manager.h"
#include "pbl/services/app_permissions/app_permissions.h"
#include "pbl/services/audio_encoder/audio_encoder.h"
#include "pbl/services/audio_endpoint.h"
#include "pbl/services/mic_capture/mic_capture_service.h"
#include "pbl/services/mic_manager.h"
#include "pbl/services/voice_endpoint.h"
#include "process_management/app_manager.h"
#include "process_management/pebble_process_md.h"

#include <string.h>

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_events.h"
#include "fake_mutex.h"
#include "fake_pbl_malloc.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_event_loop.h"
#include "stubs_mic_banner.h"
#include "fake_new_timer.h"

// Current app
////////////////////////////////////////////////////////////////

static PebbleProcessMd s_md = {.is_unprivileged = true};

const PebbleProcessMd *app_manager_get_current_app_md(void) {
  return &s_md;
}

AppInstallId app_manager_get_current_app_id(void) {
  return 1;
}

bool app_install_id_from_system(AppInstallId id) {
  return (id < 0);
}

static bool s_watchface_running;

bool app_manager_is_watchface_running(void) {
  return s_watchface_running;
}

// Fake encoder + endpoints for the phone sink
////////////////////////////////////////////////////////////////

#define FAKE_FRAME_SAMPLES (320)

static bool s_encoder_open;
static int s_encoder_open_count;
static int s_encoder_close_count;
static int s_encode_count;

bool audio_encoder_service_is_codec_available(AudioCodec codec) {
  return (codec == AudioCodecSpeexWB);
}

bool audio_encoder_service_open(AudioCodec codec, PebbleTask owner, AudioEncoderInfo *info_out) {
  if (s_encoder_open) {
    return false;
  }
  s_encoder_open = true;
  s_encoder_open_count++;
  *info_out = (AudioEncoderInfo){
    .codec = codec,
    .channels = 1,
    .frame_samples = FAKE_FRAME_SAMPLES,
    .sample_rate = 16000,
    .bitrate = 9800,
    .max_packet_bytes = 200,
    .bitstream_version = 4,
  };
  return true;
}

int audio_encoder_service_encode(PebbleTask owner, const int16_t *pcm, uint32_t num_samples,
                                 uint8_t *out, uint32_t out_len) {
  cl_assert(s_encoder_open);
  cl_assert_equal_i(num_samples, FAKE_FRAME_SAMPLES);
  s_encode_count++;
  out[0] = (uint8_t)pcm[0];
  return 25;
}

void audio_encoder_service_close(PebbleTask owner) {
  if (s_encoder_open) {
    s_encoder_close_count++;
  }
  s_encoder_open = false;
}

static AudioEndpointStopTransferCallback s_transfer_stop_cb;
static AudioEndpointSessionId s_transfer_session;
static int s_frames_sent;
static uint8_t s_last_frame_byte;
static int s_transfer_stopped_count;
static int s_transfer_cancelled_count;

AudioEndpointSessionId audio_endpoint_setup_transfer(AudioEndpointStopTransferCallback stop_cb) {
  s_transfer_stop_cb = stop_cb;
  return ++s_transfer_session;
}

void audio_endpoint_add_frame(AudioEndpointSessionId session_id, uint8_t *frame,
                              uint8_t frame_size) {
  cl_assert_equal_i(session_id, s_transfer_session);
  cl_assert_equal_i(frame_size, 25);
  s_frames_sent++;
  s_last_frame_byte = frame[0];
}

void audio_endpoint_stop_transfer(AudioEndpointSessionId session_id) {
  cl_assert_equal_i(session_id, s_transfer_session);
  s_transfer_stopped_count++;
}

void audio_endpoint_cancel_transfer(AudioEndpointSessionId session_id) {
  cl_assert_equal_i(session_id, s_transfer_session);
  s_transfer_cancelled_count++;
}

static int s_setup_sessions;
static VoiceEndpointSessionType s_setup_type;
static bool s_setup_had_uuid;
static uint16_t s_setup_frame_size;

void voice_endpoint_setup_session(VoiceEndpointSessionType session_type,
                                  AudioEndpointSessionId session_id, AudioTransferInfoSpeex *info,
                                  Uuid *app_uuid) {
  s_setup_sessions++;
  s_setup_type = session_type;
  s_setup_had_uuid = (app_uuid != NULL);
  s_setup_frame_size = info->frame_size;
  cl_assert_equal_i(session_id, s_transfer_session);
}

// Fake mic driver, driven by the tests
////////////////////////////////////////////////////////////////

MicDevice *const MIC = NULL;

static bool s_mic_running;
static MicDataHandlerCB s_mic_handler;
static void *s_mic_context;
static int16_t *s_mic_buffer;
static size_t s_mic_buffer_len;

bool mic_start(MicDevice *this, MicDataHandlerCB data_handler, void *context, int16_t *audio_buffer,
               size_t audio_buffer_len) {
  if (s_mic_running) {
    return false;
  }
  s_mic_running = true;
  s_mic_handler = data_handler;
  s_mic_context = context;
  s_mic_buffer = audio_buffer;
  s_mic_buffer_len = audio_buffer_len;
  return true;
}

void mic_stop(MicDevice *this) {
  s_mic_running = false;
  s_mic_handler = NULL;
}

//! Simulates the driver delivering one chunk of `value`-filled samples on KernelBG.
static void prv_deliver_chunk(int16_t value) {
  cl_assert(s_mic_running);
  for (size_t i = 0; i < s_mic_buffer_len; i++) {
    s_mic_buffer[i] = value;
  }
  s_mic_handler(s_mic_buffer, s_mic_buffer_len, s_mic_context);
}

// Permissions / focus
////////////////////////////////////////////////////////////////

static AppPermissionState s_permission_state;

AppPermissionState app_permissions_get_state_for_current_app(AppPermission permission) {
  cl_assert_equal_i(permission, AppPermission_Microphone);
  return s_permission_state;
}

bool app_permissions_is_granted_for_current_app(AppPermission permission) {
  return (app_permissions_get_state_for_current_app(permission) == AppPermissionStateGranted);
}

static bool s_modal_focused;

bool modal_manager_get_enabled(void) {
  return true;
}

ModalProperty modal_manager_get_properties(void) {
  return s_modal_focused ? ModalProperty_Exists : ModalPropertyDefault;
}

// Dictation, used to test preemption
////////////////////////////////////////////////////////////////

static int16_t s_dictation_buffer[320];

static void prv_dictation_handler(int16_t *samples, size_t sample_count, void *context) {
}

static bool prv_start_dictation(void) {
  return mic_manager_acquire(MicClientVoiceDictation, prv_dictation_handler, NULL,
                             s_dictation_buffer, 320, NULL, NULL);
}

// Helpers
////////////////////////////////////////////////////////////////

#define SPU (320)

static PebbleEvent prv_last_event(void) {
  return fake_event_get_last();
}

static void prv_assert_stopped_event(MicCaptureStopReason reason) {
  PebbleEvent e = prv_last_event();
  cl_assert_equal_i(e.type, PEBBLE_MIC_CAPTURE_EVENT);
  cl_assert_equal_i(e.mic_capture.type, MicCaptureEventStopped);
  cl_assert_equal_i(e.mic_capture.stop_reason, reason);
}

// Setup
////////////////////////////////////////////////////////////////

void test_mic_capture_service__initialize(void) {
  fake_event_init();
  fake_mutex_reset(false);
  fake_pbl_malloc_clear_tracking();
  s_mic_running = false;
  s_mic_handler = NULL;
  s_permission_state = AppPermissionStateGranted;
  s_modal_focused = false;
  s_md = (PebbleProcessMd){.is_unprivileged = true};
  s_watchface_running = false;
  s_encoder_open = false;
  s_encoder_open_count = 0;
  s_encoder_close_count = 0;
  s_encode_count = 0;
  s_transfer_stop_cb = NULL;
  s_frames_sent = 0;
  s_transfer_stopped_count = 0;
  s_transfer_cancelled_count = 0;
  s_setup_sessions = 0;
  s_setup_had_uuid = false;
  stub_new_timer_cleanup();
  mic_manager_init();
  mic_capture_service_init();
}

void test_mic_capture_service__cleanup(void) {
  mic_capture_service_stop_for_task(PebbleTask_App);
  mic_manager_release(MicClientVoiceDictation);
  fake_mutex_assert_all_unlocked();
  stub_new_timer_cleanup();
  fake_pbl_malloc_check_net_allocs();
}

// Tests
////////////////////////////////////////////////////////////////

void test_mic_capture_service__start_refusals(void) {
  cl_assert_equal_i(MicCaptureStartErrNotForeground,
                    mic_capture_service_start(PebbleTask_Worker, SPU));
  cl_assert_equal_i(
      MicCaptureStartErrInvalidArgs,
      mic_capture_service_start(PebbleTask_App, MIC_CAPTURE_MIN_SAMPLES_PER_UPDATE - 1));
  cl_assert_equal_i(
      MicCaptureStartErrInvalidArgs,
      mic_capture_service_start(PebbleTask_App, MIC_CAPTURE_MAX_SAMPLES_PER_UPDATE + 1));

  s_modal_focused = true;
  cl_assert_equal_i(MicCaptureStartErrNotForeground,
                    mic_capture_service_start(PebbleTask_App, SPU));
  s_modal_focused = false;

  // Watchfaces are refused even with the permission granted, for capture and streaming alike
  s_watchface_running = true;
  cl_assert_equal_i(MicCaptureStartErrWatchface, mic_capture_service_start(PebbleTask_App, SPU));
  cl_assert_equal_i(MicCaptureStartErrWatchface, mic_capture_service_start_stream(PebbleTask_App));
  s_watchface_running = false;

  s_permission_state = AppPermissionStateNotDeclared;
  cl_assert_equal_i(MicCaptureStartErrNotDeclared, mic_capture_service_start(PebbleTask_App, SPU));
  s_permission_state = AppPermissionStateDenied;
  cl_assert_equal_i(MicCaptureStartErrDenied, mic_capture_service_start(PebbleTask_App, SPU));
  s_permission_state = AppPermissionStateGranted;

  // Dictation owns the mic
  cl_assert(prv_start_dictation());
  cl_assert_equal_i(MicCaptureStartErrBusy, mic_capture_service_start(PebbleTask_App, SPU));
  cl_assert(!mic_capture_service_is_active());
  mic_manager_release(MicClientVoiceDictation);

  cl_assert(!s_mic_running);
  cl_assert_equal_i(0, fake_event_get_count());
}

void test_mic_capture_service__start_read_stop(void) {
  cl_assert_equal_i(MicCaptureStartOk, mic_capture_service_start(PebbleTask_App, SPU));
  cl_assert(mic_capture_service_is_active());
  cl_assert(s_mic_running);
  cl_assert_equal_i(SPU, s_mic_buffer_len);
  cl_assert_equal_i(MicClientAppCapture, mic_manager_get_owner());

  // Second start is refused
  cl_assert_equal_i(MicCaptureStartErrBusy, mic_capture_service_start(PebbleTask_App, SPU));

  cl_assert_equal_i(0, mic_capture_service_get_available());

  prv_deliver_chunk(7);
  cl_assert_equal_i(SPU, mic_capture_service_get_available());
  cl_assert_equal_i(1, fake_event_get_count());
  PebbleEvent e = prv_last_event();
  cl_assert_equal_i(e.type, PEBBLE_MIC_CAPTURE_EVENT);
  cl_assert_equal_i(e.mic_capture.type, MicCaptureEventData);
  cl_assert_equal_i(e.mic_capture.num_samples, SPU);
  cl_assert_equal_b(e.mic_capture.overrun, false);

  // More chunks before the app reads are coalesced into the pending event
  prv_deliver_chunk(8);
  prv_deliver_chunk(9);
  cl_assert_equal_i(1, fake_event_get_count());
  cl_assert_equal_i(3 * SPU, mic_capture_service_get_available());

  // Wrong task cannot read
  int16_t out[SPU];
  cl_assert_equal_i(0, mic_capture_service_read(PebbleTask_Worker, out, SPU));

  cl_assert_equal_i(SPU, mic_capture_service_read(PebbleTask_App, out, SPU));
  cl_assert_equal_i(7, out[0]);
  cl_assert_equal_i(7, out[SPU - 1]);
  cl_assert_equal_i(SPU, mic_capture_service_read(PebbleTask_App, out, SPU));
  cl_assert_equal_i(8, out[0]);
  // Short read at the end
  cl_assert_equal_i(SPU, mic_capture_service_read(PebbleTask_App, out, 2 * SPU));
  cl_assert_equal_i(9, out[0]);
  cl_assert_equal_i(0, mic_capture_service_read(PebbleTask_App, out, SPU));

  // After a read, the next chunk posts a new event
  prv_deliver_chunk(1);
  cl_assert_equal_i(2, fake_event_get_count());

  // App-initiated stop: no event
  mic_capture_service_stop(PebbleTask_Worker); // not the owner
  cl_assert(mic_capture_service_is_active());
  mic_capture_service_stop(PebbleTask_App);
  cl_assert(!mic_capture_service_is_active());
  cl_assert(!s_mic_running);
  cl_assert_equal_i(MicClientNone, mic_manager_get_owner());
  cl_assert_equal_i(2, fake_event_get_count());
  cl_assert_equal_i(0, mic_capture_service_read(PebbleTask_App, out, SPU));
}

void test_mic_capture_service__overrun_drops_newest(void) {
  cl_assert_equal_i(MicCaptureStartOk, mic_capture_service_start(PebbleTask_App, SPU));
  const int chunks_that_fit = MIC_CAPTURE_RING_SAMPLES / SPU;
  for (int i = 0; i < chunks_that_fit; i++) {
    prv_deliver_chunk(i);
  }
  cl_assert_equal_i(MIC_CAPTURE_RING_SAMPLES, mic_capture_service_get_available());
  prv_deliver_chunk(99); // dropped
  cl_assert_equal_i(MIC_CAPTURE_RING_SAMPLES, mic_capture_service_get_available());

  // Drain, the dropped chunk is not there
  int16_t out[SPU];
  for (int i = 0; i < chunks_that_fit; i++) {
    cl_assert_equal_i(SPU, mic_capture_service_read(PebbleTask_App, out, SPU));
    cl_assert_equal_i(i, out[0]);
  }
  cl_assert_equal_i(0, mic_capture_service_get_available());

  // The overrun is reported on the next data event
  prv_deliver_chunk(1);
  PebbleEvent e = prv_last_event();
  cl_assert_equal_i(e.mic_capture.type, MicCaptureEventData);
  cl_assert_equal_b(e.mic_capture.overrun, true);
  cl_assert_equal_i(SPU, mic_capture_service_read(PebbleTask_App, out, SPU));
  prv_deliver_chunk(2);
  e = prv_last_event();
  cl_assert_equal_b(e.mic_capture.overrun, false);
}

void test_mic_capture_service__focus_lost(void) {
  // Idle: nothing happens
  mic_capture_service_handle_app_focus_lost();
  cl_assert_equal_i(0, fake_event_get_count());

  cl_assert_equal_i(MicCaptureStartOk, mic_capture_service_start(PebbleTask_App, SPU));
  mic_capture_service_handle_app_focus_lost();
  cl_assert(!mic_capture_service_is_active());
  cl_assert(!s_mic_running);
  cl_assert_equal_i(MicClientNone, mic_manager_get_owner());
  prv_assert_stopped_event(MicCaptureStopReasonFocusLost);
}

void test_mic_capture_service__permission_revoked(void) {
  cl_assert_equal_i(MicCaptureStartOk, mic_capture_service_start(PebbleTask_App, SPU));

  // Still granted: nothing happens
  mic_capture_service_handle_permission_changed();
  cl_assert(mic_capture_service_is_active());
  cl_assert_equal_i(0, fake_event_get_count());

  s_permission_state = AppPermissionStateDenied;
  mic_capture_service_handle_permission_changed();
  cl_assert(!mic_capture_service_is_active());
  cl_assert(!s_mic_running);
  prv_assert_stopped_event(MicCaptureStopReasonPermissionRevoked);
}

void test_mic_capture_service__preempted_by_dictation(void) {
  cl_assert_equal_i(MicCaptureStartOk, mic_capture_service_start(PebbleTask_App, SPU));
  prv_deliver_chunk(3);

  cl_assert(prv_start_dictation());
  cl_assert(!mic_capture_service_is_active());
  cl_assert_equal_i(MicClientVoiceDictation, mic_manager_get_owner());
  cl_assert(s_mic_running);
  cl_assert(s_mic_buffer == s_dictation_buffer);
  prv_assert_stopped_event(MicCaptureStopReasonPreempted);

  // Late reads after preemption yield nothing
  int16_t out[SPU];
  cl_assert_equal_i(0, mic_capture_service_read(PebbleTask_App, out, SPU));
  cl_assert_equal_i(0, mic_capture_service_get_available());
}

void test_mic_capture_service__stop_for_task(void) {
  cl_assert_equal_i(MicCaptureStartOk, mic_capture_service_start(PebbleTask_App, SPU));
  prv_deliver_chunk(3);
  const uint32_t events = fake_event_get_count();

  mic_capture_service_stop_for_task(PebbleTask_Worker);
  cl_assert(mic_capture_service_is_active());

  mic_capture_service_stop_for_task(PebbleTask_App);
  cl_assert(!mic_capture_service_is_active());
  cl_assert(!s_mic_running);
  cl_assert_equal_i(events, fake_event_get_count());

  // Can start again afterwards
  cl_assert_equal_i(MicCaptureStartOk, mic_capture_service_start(PebbleTask_App, SPU));
}

// Streaming to the phone
////////////////////////////////////////////////////////////////

static void prv_start_stream_ok(void) {
  cl_assert_equal_i(MicCaptureStartOk, mic_capture_service_start_stream(PebbleTask_App));
  cl_assert(mic_capture_service_is_active());
  // Session requested, mic not started until the phone answers
  cl_assert_equal_i(1, s_setup_sessions);
  cl_assert_equal_i(s_setup_type, VoiceEndpointSessionTypeAudioStream);
  cl_assert_equal_i(s_setup_frame_size, FAKE_FRAME_SAMPLES);
  cl_assert_equal_i(1, s_encoder_open_count);
  cl_assert(!s_mic_running);
  cl_assert_equal_i(0, fake_event_get_count());
}

void test_mic_capture_service__stream_setup_and_data(void) {
  prv_start_stream_ok();
  cl_assert(s_setup_had_uuid); // a third-party app tags the session with its UUID

  mic_capture_service_handle_stream_setup_result(VoiceEndpointResultSuccess);
  cl_assert(s_mic_running);
  cl_assert_equal_i(s_mic_buffer_len, FAKE_FRAME_SAMPLES);
  cl_assert_equal_i(1, fake_event_get_count());
  PebbleEvent e = prv_last_event();
  cl_assert_equal_i(e.mic_capture.type, MicCaptureEventStarted);

  // Chunks are encoded and forwarded, never buffered for the app
  prv_deliver_chunk(42);
  prv_deliver_chunk(43);
  cl_assert_equal_i(2, s_encode_count);
  cl_assert_equal_i(2, s_frames_sent);
  cl_assert_equal_i(43, s_last_frame_byte);
  cl_assert_equal_i(0, mic_capture_service_get_available());
  int16_t out[8];
  cl_assert_equal_i(0, mic_capture_service_read(PebbleTask_App, out, 8));
  cl_assert_equal_i(1, fake_event_get_count());

  // A duplicate setup answer is ignored
  mic_capture_service_handle_stream_setup_result(VoiceEndpointResultSuccess);
  cl_assert_equal_i(1, fake_event_get_count());

  // App stops: transfer is ended politely, encoder closed, no event
  mic_capture_service_stop(PebbleTask_App);
  cl_assert(!mic_capture_service_is_active());
  cl_assert(!s_mic_running);
  cl_assert_equal_i(1, s_transfer_stopped_count);
  cl_assert_equal_i(0, s_transfer_cancelled_count);
  cl_assert_equal_i(1, s_encoder_close_count);
  cl_assert_equal_i(1, fake_event_get_count());
}

void test_mic_capture_service__stream_refused_by_phone(void) {
  prv_start_stream_ok();
  mic_capture_service_handle_stream_setup_result(VoiceEndpointResultFailDisabled);
  cl_assert(!mic_capture_service_is_active());
  cl_assert(!s_mic_running);
  cl_assert_equal_i(1, s_transfer_cancelled_count);
  cl_assert_equal_i(1, s_encoder_close_count);
  prv_assert_stopped_event(MicCaptureStopReasonPhone);
}

void test_mic_capture_service__stream_setup_timeout(void) {
  prv_start_stream_ok();
  cl_assert(stub_new_timer_fire(stub_new_timer_get_next()));
  cl_assert(!mic_capture_service_is_active());
  cl_assert_equal_i(1, s_transfer_cancelled_count);
  prv_assert_stopped_event(MicCaptureStopReasonPhone);
}

void test_mic_capture_service__stream_stopped_by_phone(void) {
  prv_start_stream_ok();
  mic_capture_service_handle_stream_setup_result(VoiceEndpointResultSuccess);
  cl_assert(s_mic_running);

  s_transfer_stop_cb(s_transfer_session);
  cl_assert(!mic_capture_service_is_active());
  cl_assert(!s_mic_running);
  // The endpoint already tore its side down; we must not send a stop back
  cl_assert_equal_i(0, s_transfer_stopped_count);
  cl_assert_equal_i(0, s_transfer_cancelled_count);
  prv_assert_stopped_event(MicCaptureStopReasonPhone);
}

void test_mic_capture_service__stream_preempted_by_system(void) {
  prv_start_stream_ok();
  mic_capture_service_handle_stream_setup_result(VoiceEndpointResultSuccess);

  mic_capture_service_handle_system_preempt();
  cl_assert(!mic_capture_service_is_active());
  cl_assert(!s_mic_running);
  cl_assert_equal_i(MicClientNone, mic_manager_get_owner());
  cl_assert_equal_i(1, s_transfer_stopped_count);
  cl_assert_equal_i(1, s_encoder_close_count);
  prv_assert_stopped_event(MicCaptureStopReasonPreempted);
}

void test_mic_capture_service__stream_refusals(void) {
  s_permission_state = AppPermissionStateDenied;
  cl_assert_equal_i(MicCaptureStartErrDenied, mic_capture_service_start_stream(PebbleTask_App));
  s_permission_state = AppPermissionStateGranted;

  cl_assert(prv_start_dictation());
  cl_assert_equal_i(MicCaptureStartErrBusy, mic_capture_service_start_stream(PebbleTask_App));
  mic_manager_release(MicClientVoiceDictation);

  // Capture and stream are exclusive
  cl_assert_equal_i(MicCaptureStartOk, mic_capture_service_start(PebbleTask_App, SPU));
  cl_assert_equal_i(MicCaptureStartErrBusy, mic_capture_service_start_stream(PebbleTask_App));
  mic_capture_service_stop(PebbleTask_App);
  cl_assert_equal_i(0, s_setup_sessions);

  // System apps stream without a UUID tag
  s_md.is_unprivileged = false;
  prv_start_stream_ok();
  cl_assert(!s_setup_had_uuid);
}
