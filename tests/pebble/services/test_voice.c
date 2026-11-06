/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/pp/session.h>
#include <pbl/services/audio_endpoint.h>
#include <pbl/services/mic_manager.h>
#include <pbl/services/voice/voice.h>
#include <pbl/services/voice/voice_speex.h>
#include <pbl/services/voice_endpoint.h>

#include <clar.h>
#include <fff.h>
#include <kernel/events.h>
#include <process_management/app_manager.h>
#include <process_management/pebble_process_md.h>

// Fakes
////////////////////////////////////////////////////////////////
#include <fake_events.h>
#include <fake_mutex.h>
#include <fake_pebble_tasks.h>
#include <fake_work.h>

// Stubs
////////////////////////////////////////////////////////////////
#include <stubs_logging.h>

#define TIMEOUT_SESSION_SETUP  (8000)
#define TIMEOUT_SESSION_RESULT (15000)

#define AUDIO_SESSION_ID (0x2211)

FAKE_VOID_FUNC(audio_endpoint_add_frame, AudioEndpointSessionId, uint8_t *, uint8_t);
FAKE_VOID_FUNC(audio_endpoint_stop_transfer, AudioEndpointSessionId);
FAKE_VOID_FUNC(audio_endpoint_cancel_transfer, AudioEndpointSessionId);
FAKE_VALUE_FUNC(bool, mic_manager_acquire, MicClient, MicDataHandlerCB, void *, int16_t *, size_t,
                MicManagerPreemptedCb, void *);
FAKE_VOID_FUNC(mic_manager_release, MicClient);
FAKE_VOID_FUNC(mic_manager_set_dictation_active, bool);
FAKE_VOID_FUNC(voice_endpoint_setup_session, VoiceEndpointSessionType, AudioEndpointSessionId,
               AudioTransferInfoSpeex *, Uuid *);
FAKE_VOID_FUNC(pbl_pp_session_set_responsiveness, struct pbl_pp_session *, enum pbl_bt_consumer,
               enum pbl_bt_response_time_state, uint16_t, pbl_bt_responsiveness_granted_cb_t);

#define FAKES(FAKE)                      \
  FAKE(audio_endpoint_add_frame)         \
  FAKE(audio_endpoint_stop_transfer)     \
  FAKE(audio_endpoint_cancel_transfer)   \
  FAKE(mic_manager_acquire)              \
  FAKE(mic_manager_release)              \
  FAKE(mic_manager_set_dictation_active) \
  FAKE(voice_endpoint_setup_session)     \
  FAKE(pbl_pp_session_set_responsiveness)

static AudioEndpointSessionId s_audio_session_id;
static AudioEndpointStopTransferCallback s_audio_stopped_cb;

AudioEndpointSessionId audio_endpoint_setup_transfer(AudioEndpointStopTransferCallback cb) {
  s_audio_stopped_cb = cb;
  return s_audio_session_id;
}

// App manager: the current app, when a test runs the service from an app
////////////////////////////////////////////////////////////////

#define APP_UUID                                                                               \
  UuidMake(0xa8, 0xc5, 0x63, 0x17, 0xa2, 0x89, 0x46, 0x5c, 0xbe, 0xf1, 0x5b, 0x98, 0x0d, 0xfd, \
           0xb0, 0x8a)

static const PebbleProcessMd s_app_md = {.uuid = APP_UUID};

const PebbleProcessMd *app_manager_get_current_app_md(void) {
  return &s_app_md;
}

AppInstallId app_manager_get_current_app_id(void) {
  return 1; // An installed app, not a system app
}

// Speex: pretend the encoder is always ready
////////////////////////////////////////////////////////////////

static int16_t s_frame_buffer[320];

bool voice_speex_is_initialized(void) {
  return true;
}

bool voice_speex_init(void) {
  return true;
}

void voice_speex_get_transfer_info(AudioTransferInfoSpeex *info) {
  *info = (AudioTransferInfoSpeex){.sample_rate = 16000, .bit_rate = 12800, .frame_size = 320};
}

int voice_speex_get_frame_size(void) {
  return sizeof(s_frame_buffer) / sizeof(s_frame_buffer[0]);
}

int16_t *voice_speex_get_frame_buffer(void) {
  return s_frame_buffer;
}

static int s_encoded_size;

int voice_speex_encode_frame(int16_t *samples, uint8_t *encoded_data, size_t max_encoded_size) {
  return s_encoded_size;
}

// Helpers
////////////////////////////////////////////////////////////////

static VoiceSessionId s_session_id;

// The service runs one timeout at a time, so the next pending item is the one in progress
static bool prv_timeout_is_scheduled(void) {
  return fake_work_get_next() != nullptr;
}

static uint32_t prv_timeout_ms(void) {
  struct pbl_delayed_work *timeout = fake_work_get_next();
  cl_assert(timeout != nullptr);
  return fake_work_timeout_ms(timeout);
}

static void prv_fire_timeout(void) {
  struct pbl_delayed_work *timeout = fake_work_get_next();
  cl_assert(timeout != nullptr);
  fake_work_fire(timeout);
}

// Starts a system session. The audio endpoint is ready at once; the voice endpoint setup result
// is still pending, so the session setup timeout is running.
static void prv_start_session_of_type(VoiceEndpointSessionType type) {
  s_session_id = voice_start_dictation(type);
  cl_assert_equal_i(s_session_id, AUDIO_SESSION_ID);
  cl_assert_equal_i(voice_endpoint_setup_session_fake.call_count, 1);
  cl_assert_equal_i(voice_endpoint_setup_session_fake.arg0_val, type);
  cl_assert_equal_i(voice_endpoint_setup_session_fake.arg1_val, AUDIO_SESSION_ID);
  cl_assert_equal_i(mic_manager_set_dictation_active_fake.arg0_val, true);

  cl_assert(prv_timeout_is_scheduled());
  cl_assert_equal_i(prv_timeout_ms(), TIMEOUT_SESSION_SETUP);
}

static void prv_start_session(void) {
  prv_start_session_of_type(VoiceEndpointSessionTypeDictation);
}

// Completes the session setup: the session is recording and no timeout is running.
static void prv_start_recording(void) {
  prv_start_session();
  fake_event_reset_count();

  const bool from_app = (pebble_task_get_current() == PebbleTask_App);
  voice_handle_session_setup_result(VoiceEndpointResultSuccess, VoiceEndpointSessionTypeDictation,
                                    from_app);
  cl_assert_equal_i(mic_manager_acquire_fake.call_count, 1);
  cl_assert(!prv_timeout_is_scheduled());

  cl_assert_equal_i(fake_event_get_count(), 1);
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_VOICE_SERVICE_EVENT);
  cl_assert_equal_i(event.voice_service.type, VoiceEventTypeSessionSetup);
  cl_assert_equal_i(event.voice_service.status, VoiceStatusSuccess);
}

// Stops the recording: the session waits for the result and the result timeout is running.
static void prv_wait_for_result(void) {
  prv_start_recording();
  fake_event_reset_count();

  voice_stop_dictation(s_session_id);
  cl_assert_equal_i(audio_endpoint_stop_transfer_fake.call_count, 1);
  cl_assert_equal_i(mic_manager_release_fake.call_count, 1);
  cl_assert(prv_timeout_is_scheduled());
  cl_assert_equal_i(prv_timeout_ms(), TIMEOUT_SESSION_RESULT);
  cl_assert_equal_i(fake_event_get_count(), 0);
}

static void prv_assert_retry_event(VoiceEndpointService service) {
  cl_assert_equal_i(fake_event_get_count(), 1);
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_VOICE_SERVICE_EVENT);
  cl_assert_equal_i(event.voice_service.type, VoiceEventTypeServiceRetry);
  cl_assert_equal_i(event.voice_service.status, VoiceStatusSuccess);
  cl_assert_equal_i(event.voice_service.service, service);
  cl_assert_equal_p(event.voice_service.data, nullptr);
}

static PebbleEvent prv_assert_event(VoiceEventType type, VoiceStatus status) {
  cl_assert_equal_i(fake_event_get_count(), 1);
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_VOICE_SERVICE_EVENT);
  cl_assert_equal_i(event.voice_service.type, type);
  cl_assert_equal_i(event.voice_service.status, status);
  return event;
}

static void prv_assert_timeout_event(VoiceEventType type) {
  PebbleEvent event = prv_assert_event(type, VoiceStatusTimeout);
  cl_assert_equal_p(event.voice_service.data, nullptr);
}

// The session was torn down and the service is idle: a new session can start.
static void prv_assert_idle(void) {
  cl_assert_equal_i(mic_manager_set_dictation_active_fake.arg0_val, false);
  cl_assert(!prv_timeout_is_scheduled());
  const int setup_calls = voice_endpoint_setup_session_fake.call_count;
  cl_assert_equal_i(voice_start_dictation(VoiceEndpointSessionTypeDictation), AUDIO_SESSION_ID);
  cl_assert_equal_i(voice_endpoint_setup_session_fake.call_count, setup_calls + 1);
  voice_cancel_dictation(AUDIO_SESSION_ID);
}

// Packed transcription: one sentence, "Hello world!" with a backspace before the punctuation
static const uint8_t s_transcription[] = {
  0x01,                                         // Transcription type: sentence list
  0x01,                                         // Sentence count
  0x03, 0x00,                                   // Word count
  85,   0x05, 0x00, 'H',    'e', 'l', 'l', 'o', // Word #1: confidence, length, data
  74,   0x05, 0x00, 'w',    'o', 'r', 'l', 'd', // Word #2
  90,   0x02, 0x00, '\x08', '!',                // Word #3: punctuation
};

// Setup and teardown
////////////////////////////////////////////////////////////////

void test_voice__initialize(void) {
  FAKES(RESET_FAKE);
  FFF_RESET_HISTORY();
  mic_manager_acquire_fake.return_val = true;

  fake_event_init();
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  s_audio_session_id = AUDIO_SESSION_ID;
  s_audio_stopped_cb = nullptr;
  s_encoded_size = 0;
  s_session_id = VOICE_SESSION_ID_INVALID;
}

void test_voice__cleanup(void) {
  // Leave the service idle for the next test
  fake_work_set_running(nullptr);
  mic_manager_acquire_fake.return_val = true;
  s_audio_session_id = AUDIO_SESSION_ID;
  voice_cancel_dictation(s_session_id);
  prv_assert_idle();
  fake_work_cleanup();
  fake_mutex_assert_all_unlocked();
  fake_mutex_reset(true);
}

// Tests: session setup
////////////////////////////////////////////////////////////////

void test_voice__start_rejects_invalid_session_type(void) {
  cl_assert_equal_i(voice_start_dictation(VoiceEndpointSessionTypeAudioStream),
                    VOICE_SESSION_ID_INVALID);
  cl_assert_equal_i(voice_start_dictation(VoiceEndpointSessionTypeCount), VOICE_SESSION_ID_INVALID);
  cl_assert_equal_i(voice_endpoint_setup_session_fake.call_count, 0);
  cl_assert_equal_i(mic_manager_set_dictation_active_fake.call_count, 0);
}

void test_voice__start_rejected_while_session_in_progress(void) {
  prv_start_session();

  cl_assert_equal_i(voice_start_dictation(VoiceEndpointSessionTypeDictation),
                    VOICE_SESSION_ID_INVALID);
  cl_assert_equal_i(voice_start_dictation(VoiceEndpointSessionTypeCommand),
                    VOICE_SESSION_ID_INVALID);
  cl_assert_equal_i(voice_endpoint_setup_session_fake.call_count, 1);

  // The first session is still waiting for its setup result
  cl_assert(prv_timeout_is_scheduled());
  cl_assert_equal_i(prv_timeout_ms(), TIMEOUT_SESSION_SETUP);
}

void test_voice__start_fails_when_audio_endpoint_busy(void) {
  s_audio_session_id = AUDIO_ENDPOINT_SESSION_INVALID_ID;

  cl_assert_equal_i(voice_start_dictation(VoiceEndpointSessionTypeDictation),
                    VOICE_SESSION_ID_INVALID);
  cl_assert_equal_i(voice_endpoint_setup_session_fake.call_count, 0);
  cl_assert_equal_i(fake_event_get_count(), 0);

  // The service is idle again, not stuck holding the mic
  s_audio_session_id = AUDIO_SESSION_ID;
  prv_assert_idle();
}

void test_voice__start_from_app_sends_app_uuid(void) {
  stub_pebble_tasks_set_current(PebbleTask_App);
  prv_start_session();

  const Uuid app_uuid = APP_UUID;
  cl_assert(voice_endpoint_setup_session_fake.arg3_val != nullptr);
  cl_assert(uuid_equal(voice_endpoint_setup_session_fake.arg3_val, &app_uuid));

  // A setup result for a system session does not match the app session
  voice_handle_session_setup_result(VoiceEndpointResultSuccess, VoiceEndpointSessionTypeDictation,
                                    false);
  prv_assert_event(VoiceEventTypeSessionSetup, VoiceStatusErrorGeneric);
  cl_assert_equal_i(mic_manager_acquire_fake.call_count, 0);
  cl_assert_equal_i(audio_endpoint_cancel_transfer_fake.call_count, 1);
  prv_assert_idle();
}

void test_voice__start_from_system_sends_no_uuid(void) {
  prv_start_session();
  cl_assert_equal_p(voice_endpoint_setup_session_fake.arg3_val, nullptr);
}

void test_voice__setup_result_starts_recording(void) {
  prv_start_recording();

  // The mic was taken for dictation with the encoder's frame buffer
  cl_assert_equal_i(mic_manager_acquire_fake.arg0_val, MicClientVoiceDictation);
  cl_assert_equal_p(mic_manager_acquire_fake.arg3_val, s_frame_buffer);
  cl_assert_equal_i(mic_manager_acquire_fake.arg4_val, voice_speex_get_frame_size());
}

void test_voice__setup_result_failure_cancels_session(void) {
  const struct {
    VoiceEndpointResult result;
    VoiceStatus status;
  } cases[] = {
    {VoiceEndpointResultFailServiceUnavailable, VoiceStatusErrorConnectivity},
    {VoiceEndpointResultFailDisabled, VoiceStatusErrorDisabled},
    {VoiceEndpointResultFailInvalidRecognizerResponse, VoiceStatusRecognizerResponseError},
    {VoiceEndpointResultFailTimeout, VoiceStatusErrorGeneric},
    {VoiceEndpointResultFailRecognizerError, VoiceStatusErrorGeneric},
    {VoiceEndpointResultFailInvalidMessage, VoiceStatusErrorGeneric},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    RESET_FAKE(voice_endpoint_setup_session);
    fake_event_reset_count();
    prv_start_session();

    voice_handle_session_setup_result(cases[i].result, VoiceEndpointSessionTypeDictation, false);
    prv_assert_event(VoiceEventTypeSessionSetup, cases[i].status);
    cl_assert_equal_i(mic_manager_acquire_fake.call_count, 0);
    cl_assert_equal_i(audio_endpoint_cancel_transfer_fake.call_count, i + 1);
    cl_assert(!prv_timeout_is_scheduled());
    cl_assert_equal_i(mic_manager_set_dictation_active_fake.arg0_val, false);
  }
}

void test_voice__setup_result_for_other_session_type_cancels_session(void) {
  prv_start_session();

  voice_handle_session_setup_result(VoiceEndpointResultSuccess, VoiceEndpointSessionTypeCommand,
                                    false);
  prv_assert_event(VoiceEventTypeSessionSetup, VoiceStatusErrorGeneric);
  cl_assert_equal_i(mic_manager_acquire_fake.call_count, 0);
  cl_assert_equal_i(audio_endpoint_cancel_transfer_fake.call_count, 1);
  prv_assert_idle();
}

void test_voice__setup_result_fails_when_mic_unavailable(void) {
  prv_start_session();
  mic_manager_acquire_fake.return_val = false;

  voice_handle_session_setup_result(VoiceEndpointResultSuccess, VoiceEndpointSessionTypeDictation,
                                    false);
  cl_assert_equal_i(mic_manager_acquire_fake.call_count, 1);
  prv_assert_event(VoiceEventTypeSessionSetup, VoiceStatusErrorGeneric);
  cl_assert_equal_i(audio_endpoint_cancel_transfer_fake.call_count, 1);
  prv_assert_idle();
}

void test_voice__setup_result_ignored_when_idle(void) {
  voice_handle_session_setup_result(VoiceEndpointResultSuccess, VoiceEndpointSessionTypeDictation,
                                    false);
  cl_assert_equal_i(fake_event_get_count(), 0);
  cl_assert_equal_i(mic_manager_acquire_fake.call_count, 0);
}

void test_voice__setup_result_while_recording_ends_session(void) {
  prv_start_recording();
  fake_event_reset_count();

  // A second setup result is unexpected: the session is dropped as a failed result
  voice_handle_session_setup_result(VoiceEndpointResultSuccess, VoiceEndpointSessionTypeDictation,
                                    false);
  prv_assert_event(VoiceEventTypeSessionResult, VoiceStatusErrorGeneric);
  cl_assert_equal_i(mic_manager_release_fake.call_count, 1);
  cl_assert_equal_i(audio_endpoint_cancel_transfer_fake.call_count, 1);
  prv_assert_idle();
}

void test_voice__setup_timeout_cancels_session(void) {
  prv_start_session();

  prv_fire_timeout();
  prv_assert_timeout_event(VoiceEventTypeSessionSetup);
  cl_assert_equal_i(audio_endpoint_cancel_transfer_fake.call_count, 1);
  cl_assert_equal_i(mic_manager_release_fake.call_count, 1);
  prv_assert_idle();

  // A late setup result finds no session
  fake_event_reset_count();
  voice_handle_session_setup_result(VoiceEndpointResultSuccess, VoiceEndpointSessionTypeDictation,
                                    false);
  cl_assert_equal_i(fake_event_get_count(), 0);
  cl_assert_equal_i(mic_manager_acquire_fake.call_count, 0);
}

// Tests: recording
////////////////////////////////////////////////////////////////

void test_voice__audio_frames_are_encoded_and_sent(void) {
  prv_start_recording();
  MicDataHandlerCB handler = mic_manager_acquire_fake.arg1_val;
  void *context = mic_manager_acquire_fake.arg2_val;
  cl_assert(handler != nullptr);

  s_encoded_size = 42;
  handler(s_frame_buffer, voice_speex_get_frame_size(), context);
  cl_assert_equal_i(audio_endpoint_add_frame_fake.call_count, 1);
  cl_assert_equal_i(audio_endpoint_add_frame_fake.arg0_val, AUDIO_SESSION_ID);
  cl_assert_equal_i(audio_endpoint_add_frame_fake.arg2_val, 42);

  // A short frame is dropped
  handler(s_frame_buffer, voice_speex_get_frame_size() - 1, context);
  cl_assert_equal_i(audio_endpoint_add_frame_fake.call_count, 1);

  // A frame the encoder rejects is dropped
  s_encoded_size = 0;
  handler(s_frame_buffer, voice_speex_get_frame_size(), context);
  cl_assert_equal_i(audio_endpoint_add_frame_fake.call_count, 1);

  // Audio arriving after the recording stopped is dropped
  s_encoded_size = 42;
  voice_stop_dictation(s_session_id);
  handler(s_frame_buffer, voice_speex_get_frame_size(), context);
  cl_assert_equal_i(audio_endpoint_add_frame_fake.call_count, 1);
}

void test_voice__phone_stops_transfer_while_recording(void) {
  prv_start_recording();
  fake_event_reset_count();
  cl_assert(s_audio_stopped_cb != nullptr);

  // Another session is not ours
  s_audio_stopped_cb(AUDIO_SESSION_ID + 1);
  cl_assert_equal_i(mic_manager_release_fake.call_count, 0);

  // The phone ended the transfer: the recording stops and the result is awaited
  s_audio_stopped_cb(AUDIO_SESSION_ID);
  cl_assert_equal_i(mic_manager_release_fake.call_count, 1);
  cl_assert_equal_i(audio_endpoint_stop_transfer_fake.call_count, 1);
  cl_assert(prv_timeout_is_scheduled());
  cl_assert_equal_i(prv_timeout_ms(), TIMEOUT_SESSION_RESULT);
  cl_assert_equal_i(fake_event_get_count(), 0);

  // Repeated stops are ignored
  s_audio_stopped_cb(AUDIO_SESSION_ID);
  cl_assert_equal_i(mic_manager_release_fake.call_count, 1);
}

void test_voice__stop_before_recording_cancels_session(void) {
  prv_start_session();

  voice_stop_dictation(s_session_id);
  cl_assert_equal_i(audio_endpoint_cancel_transfer_fake.call_count, 1);
  cl_assert_equal_i(audio_endpoint_stop_transfer_fake.call_count, 0);
  cl_assert_equal_i(mic_manager_release_fake.call_count, 0);
  cl_assert_equal_i(fake_event_get_count(), 0);
  prv_assert_idle();
}

void test_voice__stop_and_cancel_ignore_other_sessions(void) {
  prv_start_recording();

  voice_stop_dictation(AUDIO_SESSION_ID + 1);
  voice_stop_dictation(VOICE_SESSION_ID_INVALID);
  voice_cancel_dictation(AUDIO_SESSION_ID + 1);
  voice_cancel_dictation(VOICE_SESSION_ID_INVALID);
  cl_assert_equal_i(mic_manager_release_fake.call_count, 0);
  cl_assert_equal_i(audio_endpoint_stop_transfer_fake.call_count, 0);
  cl_assert_equal_i(audio_endpoint_cancel_transfer_fake.call_count, 0);
  cl_assert(!prv_timeout_is_scheduled());
}

void test_voice__cancel_while_recording(void) {
  prv_start_recording();
  fake_event_reset_count();

  voice_cancel_dictation(s_session_id);
  cl_assert_equal_i(mic_manager_release_fake.call_count, 1);
  cl_assert_equal_i(audio_endpoint_stop_transfer_fake.call_count, 1);
  cl_assert_equal_i(fake_event_get_count(), 0);
  prv_assert_idle();
}

void test_voice__cancel_while_waiting_for_result(void) {
  prv_wait_for_result();

  voice_cancel_dictation(s_session_id);
  cl_assert(!prv_timeout_is_scheduled());
  cl_assert_equal_i(fake_event_get_count(), 0);
  prv_assert_idle();

  // The result the phone still sends finds no session
  voice_handle_dictation_result(VoiceEndpointResultSuccess, AUDIO_SESSION_ID,
                                (Transcription *)s_transcription, false, nullptr);
  cl_assert_equal_i(fake_event_get_count(), 0);
}

// Tests: results
////////////////////////////////////////////////////////////////

void test_voice__dictation_result_builds_sentence(void) {
  prv_wait_for_result();

  voice_handle_dictation_result(VoiceEndpointResultSuccess, AUDIO_SESSION_ID,
                                (Transcription *)s_transcription, false, nullptr);
  PebbleEvent event = prv_assert_event(VoiceEventTypeSessionResult, VoiceStatusSuccess);
  cl_assert(event.voice_service.data != nullptr);
  cl_assert_equal_s(event.voice_service.data->sentence, "Hello world!");
  pbl_free(event.voice_service.data);

  cl_assert(!prv_timeout_is_scheduled());
  prv_assert_idle();
}

void test_voice__dictation_result_failure(void) {
  prv_wait_for_result();

  voice_handle_dictation_result(VoiceEndpointResultFailServiceUnavailable, AUDIO_SESSION_ID,
                                nullptr, false, nullptr);
  PebbleEvent event = prv_assert_event(VoiceEventTypeSessionResult, VoiceStatusErrorConnectivity);
  cl_assert_equal_p(event.voice_service.data, nullptr);
  cl_assert(!prv_timeout_is_scheduled());
  prv_assert_idle();
}

void test_voice__dictation_result_for_wrong_session_fails(void) {
  prv_wait_for_result();

  voice_handle_dictation_result(VoiceEndpointResultSuccess, AUDIO_SESSION_ID + 1,
                                (Transcription *)s_transcription, false, nullptr);
  PebbleEvent event = prv_assert_event(VoiceEventTypeSessionResult, VoiceStatusErrorGeneric);
  cl_assert_equal_p(event.voice_service.data, nullptr);
  prv_assert_idle();
}

void test_voice__dictation_result_for_app_session_requires_matching_app(void) {
  stub_pebble_tasks_set_current(PebbleTask_App);
  prv_wait_for_result();

  // A result for a system session, or for another app, is not ours
  voice_handle_dictation_result(VoiceEndpointResultSuccess, AUDIO_SESSION_ID,
                                (Transcription *)s_transcription, false, nullptr);
  prv_assert_event(VoiceEventTypeSessionResult, VoiceStatusErrorGeneric);
  prv_assert_idle();

  RESET_FAKE(voice_endpoint_setup_session);
  RESET_FAKE(mic_manager_acquire);
  RESET_FAKE(mic_manager_release);
  RESET_FAKE(audio_endpoint_stop_transfer);
  mic_manager_acquire_fake.return_val = true;
  fake_event_reset_count();
  prv_wait_for_result();
  Uuid other_uuid = UUID_INVALID;
  voice_handle_dictation_result(VoiceEndpointResultSuccess, AUDIO_SESSION_ID,
                                (Transcription *)s_transcription, true, &other_uuid);
  prv_assert_event(VoiceEventTypeSessionResult, VoiceStatusErrorGeneric);
  prv_assert_idle();

  // Our app's result is delivered
  RESET_FAKE(voice_endpoint_setup_session);
  RESET_FAKE(mic_manager_acquire);
  RESET_FAKE(mic_manager_release);
  RESET_FAKE(audio_endpoint_stop_transfer);
  mic_manager_acquire_fake.return_val = true;
  fake_event_reset_count();
  prv_wait_for_result();
  Uuid app_uuid = APP_UUID;
  voice_handle_dictation_result(VoiceEndpointResultSuccess, AUDIO_SESSION_ID,
                                (Transcription *)s_transcription, true, &app_uuid);
  PebbleEvent event = prv_assert_event(VoiceEventTypeSessionResult, VoiceStatusSuccess);
  cl_assert_equal_s(event.voice_service.data->sentence, "Hello world!");
  pbl_free(event.voice_service.data);
}

void test_voice__dictation_result_while_recording_ends_session(void) {
  prv_start_recording();
  fake_event_reset_count();

  // The phone answered before the recording stopped: stop it and report the failure
  voice_handle_dictation_result(VoiceEndpointResultSuccess, AUDIO_SESSION_ID,
                                (Transcription *)s_transcription, false, nullptr);
  PebbleEvent event = prv_assert_event(VoiceEventTypeSessionResult, VoiceStatusErrorGeneric);
  cl_assert_equal_p(event.voice_service.data, nullptr);
  cl_assert_equal_i(mic_manager_release_fake.call_count, 1);
  cl_assert_equal_i(audio_endpoint_stop_transfer_fake.call_count, 1);
  prv_assert_idle();
}

void test_voice__dictation_result_ignored_when_idle(void) {
  voice_handle_dictation_result(VoiceEndpointResultSuccess, AUDIO_SESSION_ID,
                                (Transcription *)s_transcription, false, nullptr);
  cl_assert_equal_i(fake_event_get_count(), 0);
}

void test_voice__nlp_result_carries_reminder_and_timestamp(void) {
  prv_start_session_of_type(VoiceEndpointSessionTypeNLP);
  voice_handle_session_setup_result(VoiceEndpointResultSuccess, VoiceEndpointSessionTypeNLP, false);
  voice_stop_dictation(s_session_id);
  fake_event_reset_count();

  char reminder[] = "Call Phil";
  voice_handle_nlp_result(VoiceEndpointResultSuccess, AUDIO_SESSION_ID, reminder, 0x574617E8);
  PebbleEvent event = prv_assert_event(VoiceEventTypeSessionResult, VoiceStatusSuccess);
  cl_assert(event.voice_service.data != nullptr);
  cl_assert_equal_s(event.voice_service.data->sentence, "Call Phil");
  cl_assert_equal_i(event.voice_service.data->timestamp, 0x574617E8);
  pbl_free(event.voice_service.data);

  cl_assert(!prv_timeout_is_scheduled());
  prv_assert_idle();
}

void test_voice__result_timeout_ends_session(void) {
  prv_wait_for_result();

  prv_fire_timeout();
  prv_assert_timeout_event(VoiceEventTypeSessionResult);
  prv_assert_idle();

  // The result arriving after the timeout finds no session
  fake_event_reset_count();
  voice_handle_dictation_result(VoiceEndpointResultSuccess, AUDIO_SESSION_ID,
                                (Transcription *)s_transcription, false, nullptr);
  cl_assert_equal_i(fake_event_get_count(), 0);
}

// Tests: service retry
////////////////////////////////////////////////////////////////

void test_voice__service_retry_restarts_result_timeout(void) {
  prv_wait_for_result();
  const int start_calls = s_fake_work_num_arm_calls;

  // The phone switches to another service: the result timeout starts over, the UI is told
  voice_handle_service_retry(AUDIO_SESSION_ID, VoiceEndpointServiceOffline);
  cl_assert_equal_i(s_fake_work_num_arm_calls, start_calls + 1);
  cl_assert(prv_timeout_is_scheduled());
  cl_assert_equal_i(prv_timeout_ms(), TIMEOUT_SESSION_RESULT);
  prv_assert_retry_event(VoiceEndpointServiceOffline);

  // Each retry does the same
  fake_event_reset_count();
  voice_handle_service_retry(AUDIO_SESSION_ID, VoiceEndpointServiceOnline);
  cl_assert_equal_i(s_fake_work_num_arm_calls, start_calls + 2);
  cl_assert(prv_timeout_is_scheduled());
  cl_assert_equal_i(prv_timeout_ms(), TIMEOUT_SESSION_RESULT);
  prv_assert_retry_event(VoiceEndpointServiceOnline);

  // The restarted timeout still ends the session when the retry does not produce a result
  fake_event_reset_count();
  prv_fire_timeout();
  prv_assert_timeout_event(VoiceEventTypeSessionResult);
  cl_assert_equal_i(mic_manager_set_dictation_active_fake.arg0_val, false);
}

void test_voice__service_retry_ignored_while_timeout_fires(void) {
  prv_wait_for_result();
  const int start_calls = s_fake_work_num_arm_calls;

  // The result timeout is already running, blocked on the service lock: the retry must not
  // schedule it again behind it or tell the UI about a session that is about to end
  fake_work_set_running(fake_work_get_next());
  voice_handle_service_retry(AUDIO_SESSION_ID, VoiceEndpointServiceOffline);
  cl_assert_equal_i(s_fake_work_num_arm_calls, start_calls);
  cl_assert_equal_i(fake_event_get_count(), 0);
  fake_work_set_running(nullptr);

  prv_fire_timeout();
  prv_assert_timeout_event(VoiceEventTypeSessionResult);
  cl_assert(!prv_timeout_is_scheduled());
}

void test_voice__service_retry_ignored_while_recording(void) {
  prv_start_recording();
  fake_event_reset_count();
  const int start_calls = s_fake_work_num_arm_calls;

  // No phone-side timeout runs while recording: nothing to restart, nothing to show
  voice_handle_service_retry(AUDIO_SESSION_ID, VoiceEndpointServiceOffline);
  cl_assert_equal_i(s_fake_work_num_arm_calls, start_calls);
  cl_assert(!prv_timeout_is_scheduled());
  cl_assert_equal_i(fake_event_get_count(), 0);

  // The recording carries on as before
  cl_assert_equal_i(mic_manager_release_fake.call_count, 0);
  voice_stop_dictation(s_session_id);
  cl_assert_equal_i(mic_manager_release_fake.call_count, 1);
  cl_assert(prv_timeout_is_scheduled());
  cl_assert_equal_i(prv_timeout_ms(), TIMEOUT_SESSION_RESULT);
}

void test_voice__service_retry_ignored_during_setup(void) {
  prv_start_session();
  const int start_calls = s_fake_work_num_arm_calls;

  // The session setup timeout is left alone
  voice_handle_service_retry(AUDIO_SESSION_ID, VoiceEndpointServiceOffline);
  cl_assert_equal_i(s_fake_work_num_arm_calls, start_calls);
  cl_assert(prv_timeout_is_scheduled());
  cl_assert_equal_i(prv_timeout_ms(), TIMEOUT_SESSION_SETUP);
  cl_assert_equal_i(fake_event_get_count(), 0);

  prv_fire_timeout();
  prv_assert_timeout_event(VoiceEventTypeSessionSetup);
  cl_assert_equal_i(audio_endpoint_cancel_transfer_fake.call_count, 1);
}

void test_voice__service_retry_ignored_for_wrong_session(void) {
  prv_wait_for_result();
  const int start_calls = s_fake_work_num_arm_calls;

  voice_handle_service_retry(AUDIO_SESSION_ID + 1, VoiceEndpointServiceOffline);
  cl_assert_equal_i(s_fake_work_num_arm_calls, start_calls);
  cl_assert_equal_i(fake_event_get_count(), 0);

  // The session in progress is unaffected
  voice_handle_service_retry(AUDIO_SESSION_ID, VoiceEndpointServiceOffline);
  cl_assert_equal_i(s_fake_work_num_arm_calls, start_calls + 1);
  prv_assert_retry_event(VoiceEndpointServiceOffline);
}

void test_voice__service_retry_ignored_when_idle(void) {
  const int start_calls = s_fake_work_num_arm_calls;

  voice_handle_service_retry(AUDIO_SESSION_ID, VoiceEndpointServiceOffline);
  cl_assert_equal_i(s_fake_work_num_arm_calls, start_calls);
  cl_assert_equal_i(fake_event_get_count(), 0);
}

void test_voice__service_retry_ignored_after_session_ends(void) {
  prv_wait_for_result();

  // The session timed out: a late retry from the phone must not revive it
  prv_fire_timeout();
  prv_assert_timeout_event(VoiceEventTypeSessionResult);
  fake_event_reset_count();
  const int start_calls = s_fake_work_num_arm_calls;

  voice_handle_service_retry(AUDIO_SESSION_ID, VoiceEndpointServiceOffline);
  cl_assert_equal_i(s_fake_work_num_arm_calls, start_calls);
  cl_assert(!prv_timeout_is_scheduled());
  cl_assert_equal_i(fake_event_get_count(), 0);
}
