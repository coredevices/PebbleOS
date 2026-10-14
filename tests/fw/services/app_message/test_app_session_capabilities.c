/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/comm_session/app_session_capabilities.h>
#include <pbl/services/comm_session/session.h>
#include <pbl/services/settings/settings_file.h>

#include <clar.h>
#include <fff.h>
#include <process_management/pebble_process_md.h>
#include <system/status_codes.h>

static const CommSessionCapability s_live_capabilities = (CommSessionInfiniteLogDumping);

// Fakes & Stubs
////////////////////////////////////////////////////////////////////////////////////////////////////

static PebbleProcessMd s_app_md;
const PebbleProcessMd *app_manager_get_current_app_md(void) {
  return &s_app_md;
}

CommSessionCapability comm_session_get_capabilities(CommSession *session) {
  if (!session) {
    return 0;
  }
  return s_live_capabilities;
}

FAKE_VALUE_FUNC(CommSession *, comm_session_get_current_app_session);
FAKE_VALUE_FUNC(status_t, settings_file_open, SettingsFile *, const char *, int);
FAKE_VOID_FUNC(settings_file_close, SettingsFile *);
FAKE_VALUE_FUNC(status_t, settings_file_get, SettingsFile *, const void *, size_t, void *, size_t);
FAKE_VALUE_FUNC(status_t, settings_file_set, SettingsFile *, const void *, size_t, const void *,
                size_t);
FAKE_VALUE_FUNC(status_t, settings_file_delete, SettingsFile *, const void *, size_t);
FAKE_VALUE_FUNC(status_t, settings_file_rewrite, SettingsFile *, SettingsFileRewriteCallback,
                void *);

#define FAKES(FAKE)                          \
  FAKE(comm_session_get_current_app_session) \
  FAKE(settings_file_open)                   \
  FAKE(settings_file_close)                  \
  FAKE(settings_file_get)                    \
  FAKE(settings_file_set)                    \
  FAKE(settings_file_delete)                 \
  FAKE(settings_file_rewrite)

static uint64_t s_get_value;
static status_t prv_settings_file_get(SettingsFile *file, const void *key, size_t key_len,
                                      void *val_out, size_t val_out_len) {
  *((uint64_t *)val_out) = s_get_value;
  return S_SUCCESS;
}

static uint64_t s_set_value;
static status_t prv_settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                                      const void *val, size_t val_len) {
  cl_assert_equal_i(val_len, sizeof(uint64_t));
  s_set_value = *(uint64_t *)val;
  return S_SUCCESS;
}

static void prv_set_cache(uint64_t value) {
  s_get_value = value;
  settings_file_get_fake.custom_fake = prv_settings_file_get;
}

// Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////

// Tests
////////////////////////////////////////////////////////////////////////////////////////////////////

static CommSession *s_fake_app_session = (CommSession *)~0;

void test_app_session_capabilities__initialize(void) {
  FAKES(RESET_FAKE);
  FFF_RESET_HISTORY();
  settings_file_get_fake.return_val = E_DOES_NOT_EXIST;
  settings_file_set_fake.custom_fake = prv_settings_file_set;
  s_app_md = (PebbleProcessMd){};
}

void test_app_session_capabilities__cleanup(void) {
}

void test_app_session_capabilities__no_cache_file_and_not_connected(void) {
  settings_file_open_fake.return_val = E_ERROR;
  bool has_cap =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(false, has_cap);
  cl_assert_equal_i(settings_file_get_fake.call_count, 0);
  cl_assert_equal_i(settings_file_close_fake.call_count, 0);
}

void test_app_session_capabilities__no_cache_file_but_connected(void) {
  settings_file_open_fake.return_val = E_ERROR;
  comm_session_get_current_app_session_fake.return_val = s_fake_app_session;
  bool has_cap =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(true, has_cap);
  cl_assert_equal_i(settings_file_get_fake.call_count, 0);
  cl_assert_equal_i(settings_file_close_fake.call_count, 0);
}

void test_app_session_capabilities__cache_file_and_not_connected(void) {
  prv_set_cache(CommSessionInfiniteLogDumping);
  bool has_cap =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(true, has_cap);
  cl_assert_equal_i(settings_file_get_fake.call_count, 1);
  cl_assert_equal_i(settings_file_close_fake.call_count, 1);
}

void test_app_session_capabilities__cache_file_but_no_key_and_not_connected(void) {
  bool has_cap =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(false, has_cap);
  cl_assert_equal_i(settings_file_get_fake.call_count, 1);
  cl_assert_equal_i(settings_file_close_fake.call_count, 1);
}

void test_app_session_capabilities__cache_file_and_connected_new_value(void) {
  prv_set_cache(CommSessionExtendedNotificationService);
  comm_session_get_current_app_session_fake.return_val = s_fake_app_session;

  bool has_cap_infinite_log_dumping =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(true, has_cap_infinite_log_dumping);

  bool has_ext_notifications =
      comm_session_current_app_session_cache_has_capability(CommSessionExtendedNotificationService);
  cl_assert_equal_b(false, has_ext_notifications);

  // Check that cache is re-written:
  cl_assert(settings_file_set_fake.call_count > 0);
  cl_assert_equal_i(s_set_value, s_live_capabilities);

  cl_assert_equal_i(settings_file_get_fake.call_count, 2);
  cl_assert_equal_i(settings_file_close_fake.call_count, 2);
}

void test_app_session_capabilities__cache_file_and_connected_same_value(void) {
  prv_set_cache(CommSessionInfiniteLogDumping);
  comm_session_get_current_app_session_fake.return_val = s_fake_app_session;

  bool has_cap_infinite_log_dumping =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(true, has_cap_infinite_log_dumping);

  // Check that cache is NOT re-written:
  cl_assert_equal_i(settings_file_set_fake.call_count, 0);

  cl_assert_equal_i(settings_file_get_fake.call_count, 1);
  cl_assert_equal_i(settings_file_close_fake.call_count, 1);
}
