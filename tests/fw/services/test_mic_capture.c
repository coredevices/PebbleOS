/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/services/blob_db/app_db.h>
#include <pbl/services/filesystem/pfs.h>
#include <pbl/services/mic_capture.h>
#include <pbl/services/mic_manager.h>

#include <clar.h>
#include <fake_events.h>
#include <fake_mutex.h>
#include <fake_pbl_malloc.h>
#include <fake_settings_file.h>
#include <kernel/events.h>
#include <kernel/ui/modals/modal_manager.h>
#include <process_management/app_install_manager_private.h>
#include <process_management/app_manager.h>
#include <process_management/pebble_process_md.h>
#include <stubs_event_loop.h>
#include <stubs_logging.h>
#include <stubs_mic_banner.h>
#include <stubs_passert.h>
#include <stubs_pebble_tasks.h>

static const Uuid s_app = {.byte0 = 1};
static const Uuid s_other = {.byte0 = 2};
static PebbleProcessMd s_md;
static bool s_have_md;
static bool s_watchface;
static bool s_modal;
static bool s_running;
static bool s_driver_fails;
static unsigned s_starts;
static MicDataHandlerCB s_handler;
static void *s_context;
static int16_t *s_buffer;
static size_t s_buffer_len;
MicDevice *const MIC = NULL;

const PebbleProcessMd *app_manager_get_current_app_md(void) {
  return s_have_md ? &s_md : NULL;
}

bool app_manager_is_watchface_running(void) {
  return s_watchface;
}
bool modal_manager_get_enabled(void) {
  return true;
}
ModalProperty modal_manager_get_properties(void) {
  return s_modal ? ModalProperty_Exists : ModalPropertyDefault;
}

status_t pfs_remove(const char *name) {
  fake_settings_file_reset();
  return S_SUCCESS;
}

bool mic_start(MicDevice *this, MicDataHandlerCB handler, void *context, int16_t *buffer,
               size_t len) {
  if (s_running || s_driver_fails)
    return false;
  s_running = true;
  s_starts++;
  s_handler = handler;
  s_context = context;
  s_buffer = buffer;
  s_buffer_len = len;
  return true;
}

void mic_stop(MicDevice *this) {
  s_running = false;
  s_handler = NULL;
}

static void prv_deliver(int16_t value) {
  cl_assert(s_running);
  for (size_t i = 0; i < s_buffer_len; i++)
    s_buffer[i] = value;
  s_handler(s_buffer, s_buffer_len, s_context);
}

static unsigned s_install_callbacks;
void app_install_clear_app_db(void) {
}
bool app_fetch_in_progress(void) {
  return false;
}
void app_fetch_cancel_from_system_task(void) {
}
bool app_install_do_callbacks(InstallEventType type, AppInstallId id, Uuid *uuid,
                              InstallCallbackDoneCallback cb, void *data) {
  s_install_callbacks++;
  kernel_free(uuid);
  return true;
}

static void prv_snapshot(const Uuid *first, const Uuid *second) {
  const Uuid *apps[] = {&s_app, &s_other};
  for (unsigned i = 0; i < 2; i++) {
    AppDBEntry entry = {.uuid = *apps[i]};
    if ((first && uuid_equal(first, apps[i])) || (second && uuid_equal(second, apps[i]))) {
      entry.permissions = APP_DB_PERMISSION_MICROPHONE;
    }
    cl_assert_equal_i(
        S_SUCCESS, app_db_insert((uint8_t *)apps[i], UUID_SIZE, (uint8_t *)&entry, sizeof(entry)));
  }
}

static uint32_t prv_start(void) {
  prv_snapshot(&s_app, NULL);
  const int32_t session = mic_capture_service_start(PebbleTask_App);
  cl_assert(session > 0);
  cl_assert(s_running);
  cl_assert_equal_i(320, s_buffer_len);
  return (uint32_t)session;
}

static void prv_stopped(uint32_t session, MicCaptureStopReason reason) {
  cl_assert(!s_running);
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(PEBBLE_MIC_CAPTURE_EVENT, event.type);
  cl_assert_equal_i(MicCaptureEventStopped, event.mic_capture.type);
  cl_assert_equal_i(reason, event.mic_capture.stop_reason);
  cl_assert_equal_i(session, event.mic_capture.session);
  int16_t samples[320];
  cl_assert_equal_i(0, mic_capture_service_read(PebbleTask_App, session, samples));
}

void test_mic_capture__initialize(void) {
  fake_event_init();
  fake_mutex_reset(false);
  fake_settings_file_reset();
  fake_pbl_malloc_clear_tracking();
  s_md = (PebbleProcessMd){.uuid = s_app, .is_unprivileged = true};
  s_have_md = true;
  s_watchface = false;
  s_modal = false;
  s_running = false;
  s_driver_fails = false;
  s_starts = 0;
  app_db_init();
  s_install_callbacks = 0;
  mic_manager_init();
  mic_capture_service_init();
}

void test_mic_capture__cleanup(void) {
  mic_capture_service_stop_for_task(PebbleTask_App);
  mic_manager_release(MicClientVoiceDictation);
  fake_settings_file_reset();
  fake_mutex_assert_all_unlocked();
  fake_pbl_malloc_check_net_allocs();
}

void test_mic_capture__permission_required_and_grants_are_app_specific(void) {
  cl_assert_equal_i(-MicCaptureStartErrPermissionDenied, mic_capture_service_start(PebbleTask_App));
  prv_snapshot(&s_other, NULL);
  cl_assert_equal_i(-MicCaptureStartErrPermissionDenied, mic_capture_service_start(PebbleTask_App));
  cl_assert_equal_i(0, s_starts);
  const uint32_t session = prv_start();
  mic_capture_service_stop(PebbleTask_App, session);
  app_db_init();
  cl_assert(app_db_microphone_granted(&s_app));
  cl_assert(mic_capture_service_start(PebbleTask_App) > 0);
}

void test_mic_capture__raw_pcm_queue_is_bounded_and_preserves_signed_samples(void) {
  const uint32_t session = prv_start();
  prv_deliver(-1234);
  prv_deliver(2345);
  prv_deliver(32767);
  cl_assert_equal_i(1, fake_event_get_count());
  int16_t samples[320];
  cl_assert_equal_i(320, mic_capture_service_read(PebbleTask_App, session, samples));
  for (int i = 0; i < 320; i++)
    cl_assert_equal_i(-1234, samples[i]);
  cl_assert_equal_i(2, fake_event_get_count());
  cl_assert_equal_i(320, mic_capture_service_read(PebbleTask_App, session, samples));
  cl_assert_equal_i(2345, samples[319]);
  cl_assert_equal_i(0, mic_capture_service_read(PebbleTask_App, session, samples));
  prv_deliver(-42);
  cl_assert_equal_i(320, mic_capture_service_read(PebbleTask_App, session, samples));
  cl_assert_equal_i(-42, samples[0]);
}

void test_mic_capture__revocation_drops_queued_audio(void) {
  const uint32_t session = prv_start();
  prv_deliver(99);
  prv_snapshot(&s_other, NULL);
  prv_stopped(session, MicCaptureStopReasonPermissionRevoked);
  cl_assert_equal_i(-MicCaptureStartErrPermissionDenied, mic_capture_service_start(PebbleTask_App));
}

void test_mic_capture__empty_snapshot_and_clear_revoke_capture(void) {
  uint32_t session = prv_start();
  prv_snapshot(NULL, NULL);
  prv_stopped(session, MicCaptureStopReasonPermissionRevoked);
  session = prv_start();
  cl_assert_equal_i(S_SUCCESS, app_db_flush());
  prv_stopped(session, MicCaptureStopReasonPermissionRevoked);
}

void test_mic_capture__snapshot_update_preserves_a_retained_grant(void) {
  const uint32_t session = prv_start();
  prv_snapshot(&s_other, &s_app);
  cl_assert(mic_capture_service_is_active(PebbleTask_App, session));
  cl_assert_equal_i(0, fake_event_get_count());
}

void test_mic_capture__focus_loss_and_exit_stop_capture(void) {
  const uint32_t session = prv_start();
  mic_capture_service_handle_focus_lost();
  prv_stopped(session, MicCaptureStopReasonFocusLost);
  const uint32_t next = prv_start();
  cl_assert(next != session);
  s_have_md = false;
  const int event_count = fake_event_get_count();
  mic_capture_service_stop_for_task(PebbleTask_App);
  cl_assert(!s_running);
  cl_assert_equal_i(event_count, fake_event_get_count());
}

static void prv_dictation(int16_t *samples, size_t count, void *context) {
}

void test_mic_capture__dictation_preempts_capture(void) {
  const uint32_t session = prv_start();
  int16_t buffer[320];
  cl_assert(
      mic_manager_acquire(MicClientVoiceDictation, prv_dictation, NULL, buffer, 320, NULL, NULL));
  cl_assert_equal_i(MicClientVoiceDictation, mic_manager_get_owner());
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(session, event.mic_capture.session);
  cl_assert_equal_i(MicCaptureStopReasonInterrupted, event.mic_capture.stop_reason);
  mic_capture_service_stop(PebbleTask_App, session);
  cl_assert(s_running);
  cl_assert_equal_i(-MicCaptureStartErrBusy, mic_capture_service_start(PebbleTask_App));
}

void test_mic_capture__workers_watchfaces_and_obscured_apps_are_refused(void) {
  cl_assert_equal_i(-MicCaptureStartErrNotForeground, mic_capture_service_start(PebbleTask_Worker));
  s_watchface = true;
  cl_assert_equal_i(-MicCaptureStartErrWatchface, mic_capture_service_start(PebbleTask_App));
  s_watchface = false;
  s_modal = true;
  cl_assert_equal_i(-MicCaptureStartErrNotForeground, mic_capture_service_start(PebbleTask_App));
  s_modal = false;
  s_have_md = false;
  cl_assert_equal_i(-MicCaptureStartErrNotForeground, mic_capture_service_start(PebbleTask_App));
  cl_assert_equal_i(0, s_starts);
}

void test_mic_capture__a_stale_session_or_another_app_cannot_read_or_stop(void) {
  const uint32_t old = prv_start();
  mic_capture_service_stop(PebbleTask_App, old);
  const uint32_t session = prv_start();
  prv_deliver(7);
  int16_t samples[320];
  cl_assert_equal_i(0, mic_capture_service_read(PebbleTask_App, old, samples));
  mic_capture_service_stop(PebbleTask_App, old);
  cl_assert(s_running);
  cl_assert_equal_i(0, mic_capture_service_read(PebbleTask_Worker, session, samples));
  s_md.uuid = s_other;
  cl_assert_equal_i(0, mic_capture_service_read(PebbleTask_App, session, samples));
  mic_capture_service_stop(PebbleTask_App, session);
  cl_assert(s_running);
  s_md.uuid = s_app;
  cl_assert_equal_i(320, mic_capture_service_read(PebbleTask_App, session, samples));
}

void test_mic_capture__legacy_and_permission_only_metadata_updates(void) {
  uint32_t session = prv_start();
  const unsigned callbacks = s_install_callbacks;
  prv_snapshot(&s_app, &s_other);
  cl_assert_equal_i(callbacks, s_install_callbacks);
  cl_assert(mic_capture_service_is_active(PebbleTask_App, session));
  AppDBEntry legacy = {.uuid = s_app};
  cl_assert_equal_i(S_SUCCESS, app_db_insert((uint8_t *)&s_app, UUID_SIZE, (uint8_t *)&legacy,
                                             APP_DB_LEGACY_ENTRY_SIZE));
  prv_stopped(session, MicCaptureStopReasonPermissionRevoked);
  cl_assert_equal_i(callbacks, s_install_callbacks);
  cl_assert(!app_db_microphone_granted(&s_app));
}

void test_mic_capture__malformed_metadata_does_not_grant_permission(void) {
  AppDBEntry entry = {.uuid = s_app, .permissions = APP_DB_PERMISSION_MICROPHONE};
  cl_assert_equal_i(E_INVALID_ARGUMENT, app_db_insert((uint8_t *)&s_other, UUID_SIZE,
                                                      (uint8_t *)&entry, sizeof(entry)));
  cl_assert_equal_i(E_INVALID_ARGUMENT, app_db_insert((uint8_t *)&s_app, UUID_SIZE,
                                                      (uint8_t *)&entry, sizeof(entry) - 1));
  cl_assert(!app_db_microphone_granted(&s_app));
}

void test_mic_capture__removing_app_metadata_revokes_capture(void) {
  uint32_t session = prv_start();
  cl_assert_equal_i(S_SUCCESS, app_db_delete((uint8_t *)&s_app, UUID_SIZE));
  prv_stopped(session, MicCaptureStopReasonPermissionRevoked);
}

void test_mic_capture__driver_failure_leaves_no_active_capture(void) {
  prv_snapshot(&s_app, NULL);
  s_driver_fails = true;
  cl_assert_equal_i(-MicCaptureStartErrUnavailable, mic_capture_service_start(PebbleTask_App));
  cl_assert_equal_i(MicClientNone, mic_manager_get_owner());
  cl_assert(!s_running);
}
