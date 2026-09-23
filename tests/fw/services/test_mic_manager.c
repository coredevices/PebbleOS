/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/mic_manager.h"

#include <string.h>

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_mutex.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_logging.h"
#include "stubs_passert.h"

// Fake mic driver
////////////////////////////////////////////////////////////////

MicDevice *const MIC = NULL;

static bool s_mic_running;
static int s_start_count;
static int s_stop_count;
static MicDataHandlerCB s_handler;
static int16_t *s_buffer;

bool mic_start(MicDevice *this, MicDataHandlerCB data_handler, void *context, int16_t *audio_buffer,
               size_t audio_buffer_len) {
  if (s_mic_running) {
    return false;
  }
  s_mic_running = true;
  s_start_count++;
  s_handler = data_handler;
  s_buffer = audio_buffer;
  return true;
}

void mic_stop(MicDevice *this) {
  s_mic_running = false;
  s_stop_count++;
  s_handler = NULL;
  s_buffer = NULL;
}

// Clients
////////////////////////////////////////////////////////////////

static int16_t s_dictation_buffer[320];
static int16_t s_app_buffer[160];

static void prv_dictation_handler(int16_t *samples, size_t sample_count, void *context) {
}

static void prv_app_handler(int16_t *samples, size_t sample_count, void *context) {
}

static int s_preempted_count;
static MicClient s_owner_seen_in_preempt;
static bool s_reacquire_in_preempt;
static bool s_reacquire_result;

static void prv_app_preempted(void *context) {
  s_preempted_count++;
  s_owner_seen_in_preempt = mic_manager_get_owner();
  // A client that misbehaves and releases anyway must not affect the new owner
  mic_manager_release(MicClientAppCapture);
  if (s_reacquire_in_preempt) {
    s_reacquire_result = mic_manager_acquire(MicClientAppCapture, prv_app_handler, NULL,
                                             s_app_buffer, 160, prv_app_preempted, NULL);
  }
}

static bool prv_acquire_app(void) {
  return mic_manager_acquire(MicClientAppCapture, prv_app_handler, NULL, s_app_buffer, 160,
                             prv_app_preempted, NULL);
}

static bool prv_acquire_dictation(void) {
  return mic_manager_acquire(MicClientVoiceDictation, prv_dictation_handler, NULL,
                             s_dictation_buffer, 320, NULL, NULL);
}

// Setup
////////////////////////////////////////////////////////////////

void test_mic_manager__initialize(void) {
  fake_mutex_reset(false);
  s_mic_running = false;
  s_start_count = 0;
  s_stop_count = 0;
  s_handler = NULL;
  s_buffer = NULL;
  s_preempted_count = 0;
  s_owner_seen_in_preempt = MicClientNone;
  s_reacquire_in_preempt = false;
  s_reacquire_result = true;
  mic_manager_init();
}

void test_mic_manager__cleanup(void) {
  fake_mutex_assert_all_unlocked();
}

// Tests
////////////////////////////////////////////////////////////////

void test_mic_manager__acquire_release(void) {
  cl_assert_equal_i(MicClientNone, mic_manager_get_owner());

  cl_assert(prv_acquire_app());
  cl_assert_equal_i(MicClientAppCapture, mic_manager_get_owner());
  cl_assert_equal_i(1, s_start_count);
  cl_assert(s_handler == prv_app_handler);
  cl_assert(s_buffer == s_app_buffer);

  // Same client twice is refused
  cl_assert(!prv_acquire_app());
  cl_assert_equal_i(1, s_start_count);

  mic_manager_release(MicClientAppCapture);
  cl_assert_equal_i(MicClientNone, mic_manager_get_owner());
  cl_assert_equal_i(1, s_stop_count);
  cl_assert(!s_mic_running);

  // Release when not owning is a no-op
  mic_manager_release(MicClientAppCapture);
  mic_manager_release(MicClientNone);
  cl_assert_equal_i(1, s_stop_count);
}

void test_mic_manager__invalid_args(void) {
  cl_assert(
      !mic_manager_acquire(MicClientNone, prv_app_handler, NULL, s_app_buffer, 160, NULL, NULL));
  cl_assert(!mic_manager_acquire(MicClientAppCapture, NULL, NULL, s_app_buffer, 160, NULL, NULL));
  cl_assert(
      !mic_manager_acquire(MicClientAppCapture, prv_app_handler, NULL, NULL, 160, NULL, NULL));
  cl_assert(!mic_manager_acquire(MicClientAppCapture, prv_app_handler, NULL, s_app_buffer, 0, NULL,
                                 NULL));
  cl_assert_equal_i(0, s_start_count);
}

void test_mic_manager__app_refused_while_dictation_owns(void) {
  cl_assert(prv_acquire_dictation());
  cl_assert(!prv_acquire_app());
  cl_assert_equal_i(MicClientVoiceDictation, mic_manager_get_owner());
  cl_assert_equal_i(1, s_start_count);

  // App release while dictation owns changes nothing
  mic_manager_release(MicClientAppCapture);
  cl_assert(s_mic_running);
  cl_assert_equal_i(MicClientVoiceDictation, mic_manager_get_owner());

  mic_manager_release(MicClientVoiceDictation);
  cl_assert_equal_i(MicClientNone, mic_manager_get_owner());
}

void test_mic_manager__dictation_preempts_app(void) {
  cl_assert(prv_acquire_app());

  cl_assert(prv_acquire_dictation());
  cl_assert_equal_i(1, s_preempted_count);
  // The app was already released when told, so its own release was a no-op
  cl_assert_equal_i(MicClientNone, s_owner_seen_in_preempt);
  cl_assert_equal_i(MicClientVoiceDictation, mic_manager_get_owner());
  cl_assert_equal_i(2, s_start_count);
  cl_assert_equal_i(1, s_stop_count);
  cl_assert(s_handler == prv_dictation_handler);
  cl_assert(s_buffer == s_dictation_buffer);

  mic_manager_release(MicClientVoiceDictation);
  cl_assert(!s_mic_running);
}

void test_mic_manager__app_cannot_reacquire_during_preempt(void) {
  cl_assert(prv_acquire_app());
  s_reacquire_in_preempt = true;

  cl_assert(prv_acquire_dictation());
  cl_assert_equal_i(1, s_preempted_count);
  cl_assert(!s_reacquire_result);
  cl_assert_equal_i(MicClientVoiceDictation, mic_manager_get_owner());
  cl_assert(s_handler == prv_dictation_handler);
}

void test_mic_manager__driver_start_failure(void) {
  // Simulate the driver being busy outside our control
  s_mic_running = true;
  cl_assert(!prv_acquire_app());
  cl_assert_equal_i(MicClientNone, mic_manager_get_owner());
}
