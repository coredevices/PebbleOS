/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "kernel/events.h"
#include "kernel/ui/modals/modal_manager.h"
#include "pbl/services/audio_endpoint.h"
#include "pbl/services/mic_manager.h"
#include "pbl/services/mic_stream.h"
#include "pbl/services/voice/voice_speex.h"
#include "pbl/services/voice_endpoint.h"
#include "process_management/app_manager.h"
#include "process_management/pebble_process_md.h"

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_events.h"
#include "fake_mutex.h"
#include "fake_new_timer.h"
#include "fake_pbl_malloc.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_event_loop.h"
#include "stubs_logging.h"
#include "stubs_mic_banner.h"
#include "stubs_passert.h"

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

static bool s_modal_focused;

bool modal_manager_get_enabled(void) {
  return true;
}

ModalProperty modal_manager_get_properties(void) {
  return s_modal_focused ? ModalProperty_Exists : ModalPropertyDefault;
}

// Fake Speex encoder and endpoints
////////////////////////////////////////////////////////////////

#define FAKE_FRAME_SAMPLES (320)

static bool s_speex_initialized;
static bool s_speex_init_fails;
static int s_encode_count;

bool voice_speex_is_initialized(void) {
  return s_speex_initialized;
}

bool voice_speex_init(void) {
  s_speex_initialized = !s_speex_init_fails;
  return s_speex_initialized;
}

int voice_speex_get_frame_size(void) {
  return FAKE_FRAME_SAMPLES;
}

void voice_speex_get_transfer_info(AudioTransferInfoSpeex *info) {
  *info = (AudioTransferInfoSpeex){.sample_rate = 16000, .frame_size = FAKE_FRAME_SAMPLES};
}

int voice_speex_encode_frame(int16_t *samples, uint8_t *encoded_data, size_t max_encoded_size) {
  cl_assert(s_speex_initialized);
  s_encode_count++;
  encoded_data[0] = (uint8_t)samples[0];
  return 25;
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

void voice_endpoint_setup_session(VoiceEndpointSessionType session_type,
                                  AudioEndpointSessionId session_id, AudioTransferInfoSpeex *info,
                                  Uuid *app_uuid) {
  s_setup_sessions++;
  s_setup_type = session_type;
  s_setup_had_uuid = (app_uuid != NULL);
  cl_assert_equal_i(info->frame_size, FAKE_FRAME_SAMPLES);
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

// Dictation, used to test preemption
////////////////////////////////////////////////////////////////

static int16_t s_dictation_buffer[FAKE_FRAME_SAMPLES];

static void prv_dictation_handler(int16_t *samples, size_t sample_count, void *context) {
}

static bool prv_start_dictation(void) {
  return mic_manager_acquire(MicClientVoiceDictation, prv_dictation_handler, NULL,
                             s_dictation_buffer, FAKE_FRAME_SAMPLES, NULL, NULL);
}

// Helpers
////////////////////////////////////////////////////////////////

static void prv_assert_stopped_event(MicStreamServiceStopReason reason) {
  PebbleEvent e = fake_event_get_last();
  cl_assert_equal_i(e.type, PEBBLE_MIC_STREAM_EVENT);
  cl_assert_equal_i(e.mic_stream.type, MicStreamEventStopped);
  cl_assert_equal_i(e.mic_stream.stop_reason, reason);
}

static void prv_start_ok(void) {
  cl_assert_equal_i(MicStreamServiceStartOk, mic_stream_service_start(PebbleTask_App));
  cl_assert(mic_stream_service_is_active());
  // Session requested, mic not started until the phone answers
  cl_assert_equal_i(1, s_setup_sessions);
  cl_assert_equal_i(s_setup_type, VoiceEndpointSessionTypeAudioStream);
  cl_assert(!s_mic_running);
  cl_assert_equal_i(0, fake_event_get_count());
}

static void prv_start_and_accept(void) {
  prv_start_ok();
  mic_stream_service_handle_setup_result(VoiceEndpointResultSuccess);
  cl_assert(s_mic_running);
}

// Setup
////////////////////////////////////////////////////////////////

void test_mic_stream__initialize(void) {
  fake_event_init();
  fake_mutex_reset(false);
  fake_pbl_malloc_clear_tracking();
  s_md = (PebbleProcessMd){.is_unprivileged = true};
  s_watchface_running = false;
  s_modal_focused = false;
  s_speex_initialized = false;
  s_speex_init_fails = false;
  s_encode_count = 0;
  s_mic_running = false;
  s_mic_handler = NULL;
  s_transfer_stop_cb = NULL;
  s_frames_sent = 0;
  s_transfer_stopped_count = 0;
  s_transfer_cancelled_count = 0;
  s_setup_sessions = 0;
  s_setup_had_uuid = false;
  stub_new_timer_cleanup();
  mic_manager_init();
  mic_stream_service_init();
}

void test_mic_stream__cleanup(void) {
  mic_stream_service_stop_for_task(PebbleTask_App);
  mic_manager_release(MicClientVoiceDictation);
  fake_mutex_assert_all_unlocked();
  stub_new_timer_cleanup();
  fake_pbl_malloc_check_net_allocs();
}

// Tests
////////////////////////////////////////////////////////////////

void test_mic_stream__start_refusals(void) {
  cl_assert_equal_i(MicStreamServiceStartErrNotForeground,
                    mic_stream_service_start(PebbleTask_Worker));

  s_modal_focused = true;
  cl_assert_equal_i(MicStreamServiceStartErrNotForeground,
                    mic_stream_service_start(PebbleTask_App));
  s_modal_focused = false;

  s_watchface_running = true;
  cl_assert_equal_i(MicStreamServiceStartErrWatchface, mic_stream_service_start(PebbleTask_App));
  s_watchface_running = false;

  cl_assert(prv_start_dictation());
  cl_assert_equal_i(MicStreamServiceStartErrBusy, mic_stream_service_start(PebbleTask_App));
  mic_manager_release(MicClientVoiceDictation);

  s_speex_init_fails = true;
  cl_assert_equal_i(MicStreamServiceStartErrNoMemory, mic_stream_service_start(PebbleTask_App));
  s_speex_init_fails = false;

  cl_assert(!mic_stream_service_is_active());
  cl_assert_equal_i(0, s_setup_sessions);
  cl_assert_equal_i(0, fake_event_get_count());
}

void test_mic_stream__setup_and_data(void) {
  prv_start_ok();
  cl_assert(s_setup_had_uuid); // a third-party app tags the session with its UUID
  cl_assert_equal_i(MicStreamServiceStartErrBusy, mic_stream_service_start(PebbleTask_App));

  mic_stream_service_handle_setup_result(VoiceEndpointResultSuccess);
  cl_assert(s_mic_running);
  cl_assert_equal_i(FAKE_FRAME_SAMPLES, s_mic_buffer_len);
  cl_assert_equal_i(MicClientAppCapture, mic_manager_get_owner());
  cl_assert_equal_i(1, fake_event_get_count());
  PebbleEvent e = fake_event_get_last();
  cl_assert_equal_i(e.type, PEBBLE_MIC_STREAM_EVENT);
  cl_assert_equal_i(e.mic_stream.type, MicStreamEventStarted);

  prv_deliver_chunk(42);
  prv_deliver_chunk(43);
  cl_assert_equal_i(2, s_encode_count);
  cl_assert_equal_i(2, s_frames_sent);
  cl_assert_equal_i(43, s_last_frame_byte);

  // A duplicate setup answer is ignored
  mic_stream_service_handle_setup_result(VoiceEndpointResultSuccess);
  cl_assert_equal_i(1, fake_event_get_count());

  // Only the owner can stop it
  mic_stream_service_stop(PebbleTask_Worker);
  cl_assert(mic_stream_service_is_active());

  // App stops: transfer is ended politely, no event
  mic_stream_service_stop(PebbleTask_App);
  cl_assert(!mic_stream_service_is_active());
  cl_assert(!s_mic_running);
  cl_assert_equal_i(MicClientNone, mic_manager_get_owner());
  cl_assert_equal_i(1, s_transfer_stopped_count);
  cl_assert_equal_i(0, s_transfer_cancelled_count);
  cl_assert_equal_i(1, fake_event_get_count());
}

void test_mic_stream__refused_by_phone(void) {
  prv_start_ok();
  mic_stream_service_handle_setup_result(VoiceEndpointResultFailDisabled);
  cl_assert(!mic_stream_service_is_active());
  cl_assert(!s_mic_running);
  cl_assert_equal_i(1, s_transfer_cancelled_count);
  prv_assert_stopped_event(MicStreamServiceStopReasonPhone);
}

void test_mic_stream__setup_timeout(void) {
  prv_start_ok();
  cl_assert(stub_new_timer_fire(stub_new_timer_get_next()));
  cl_assert(!mic_stream_service_is_active());
  cl_assert_equal_i(1, s_transfer_cancelled_count);
  prv_assert_stopped_event(MicStreamServiceStopReasonPhone);
}

void test_mic_stream__stopped_by_phone(void) {
  prv_start_and_accept();
  s_transfer_stop_cb(s_transfer_session);
  cl_assert(!mic_stream_service_is_active());
  cl_assert(!s_mic_running);
  // The endpoint already tore its side down; we must not send a stop back
  cl_assert_equal_i(0, s_transfer_stopped_count);
  cl_assert_equal_i(0, s_transfer_cancelled_count);
  prv_assert_stopped_event(MicStreamServiceStopReasonPhone);
}

void test_mic_stream__focus_lost(void) {
  // Idle: nothing happens
  mic_stream_service_handle_app_focus_lost();
  cl_assert_equal_i(0, fake_event_get_count());

  prv_start_and_accept();
  mic_stream_service_handle_app_focus_lost();
  cl_assert(!mic_stream_service_is_active());
  cl_assert(!s_mic_running);
  cl_assert_equal_i(MicClientNone, mic_manager_get_owner());
  cl_assert_equal_i(1, s_transfer_stopped_count);
  prv_assert_stopped_event(MicStreamServiceStopReasonFocusLost);
}

void test_mic_stream__focus_lost_during_setup(void) {
  prv_start_ok();
  mic_stream_service_handle_app_focus_lost();
  cl_assert(!mic_stream_service_is_active());
  prv_assert_stopped_event(MicStreamServiceStopReasonFocusLost);
  // A late answer from the phone does not start the mic
  mic_stream_service_handle_setup_result(VoiceEndpointResultSuccess);
  cl_assert(!s_mic_running);
}

void test_mic_stream__preempted_by_dictation(void) {
  prv_start_and_accept();

  // voice_start_dictation() stops the stream before it takes the mic
  mic_stream_service_handle_system_preempt();
  cl_assert(!mic_stream_service_is_active());
  cl_assert(!s_mic_running);
  cl_assert_equal_i(1, s_transfer_stopped_count);
  prv_assert_stopped_event(MicStreamServiceStopReasonPreempted);
  cl_assert(prv_start_dictation());
}

void test_mic_stream__preempted_through_mic_manager(void) {
  prv_start_and_accept();
  cl_assert(prv_start_dictation());
  cl_assert(!mic_stream_service_is_active());
  cl_assert_equal_i(MicClientVoiceDictation, mic_manager_get_owner());
  cl_assert(s_mic_buffer == s_dictation_buffer);
  prv_assert_stopped_event(MicStreamServiceStopReasonPreempted);
}

void test_mic_stream__stop_for_task(void) {
  prv_start_and_accept();
  const uint32_t events = fake_event_get_count();

  mic_stream_service_stop_for_task(PebbleTask_Worker);
  cl_assert(mic_stream_service_is_active());

  mic_stream_service_stop_for_task(PebbleTask_App);
  cl_assert(!mic_stream_service_is_active());
  cl_assert(!s_mic_running);
  cl_assert_equal_i(events, fake_event_get_count());

  // Can start again afterwards
  cl_assert_equal_i(MicStreamServiceStartOk, mic_stream_service_start(PebbleTask_App));
  cl_assert_equal_i(2, s_setup_sessions);
}

void test_mic_stream__system_app_has_no_uuid(void) {
  s_md.is_unprivileged = false;
  prv_start_ok();
  cl_assert(!s_setup_had_uuid);
}
