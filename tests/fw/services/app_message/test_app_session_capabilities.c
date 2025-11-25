/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "services/common/comm_session/app_session_capabilities.h"
#include "services/common/comm_session/session.h"
#include "process_management/pebble_process_md.h"
#include "services/normal/settings/settings_file.h"
#include "system/status_codes.h"

static const CommSessionCapability s_live_capabilities = (CommSessionInfiniteLogDumping);

// Fakes & Stubs
////////////////////////////////////////////////////////////////////////////////////////////////////

static PebbleProcessMd s_app_md;
const PebbleProcessMd* app_manager_get_current_app_md(void) {
  return &s_app_md;
}

CommSessionCapability comm_session_get_capabilities(CommSession *session) {
  if (!session) {
    return 0;
  }
  return s_live_capabilities;
}

static CommSession *s_app_session_ptr;
CommSession *comm_session_get_current_app_session(void) {
  return s_app_session_ptr;
}

static bool s_close_called;
void settings_file_close(SettingsFile *file) {
  s_close_called = true;
}

static status_t s_open_status;
status_t settings_file_open(SettingsFile *file, const char *name,
                            int max_used_space) {
  return s_open_status;
}

static bool s_has_cache;
static bool s_get_called;
static uint64_t s_get_value;
status_t settings_file_get(SettingsFile *file, const void *key, size_t key_len,
                           void *val_out, size_t val_out_len) {
  s_get_called = true;
  if (!s_has_cache) {
    return E_DOES_NOT_EXIST;
  }
  *((uint64_t *)val_out) = s_get_value;
  return S_SUCCESS;
}

static uint64_t s_set_value;
status_t settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                           const void *val, size_t val_len) {
  cl_assert_equal_i(val_len, sizeof(uint64_t));
  s_set_value = *(uint64_t *)val;
  return S_SUCCESS;
}

status_t settings_file_delete(SettingsFile *file,
                              const void *key, size_t key_len) {
  return S_SUCCESS;
}

status_t settings_file_rewrite(SettingsFile *file,
                               SettingsFileRewriteCallback cb, void *context) {
  return S_SUCCESS;
}

// Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////



// Tests
////////////////////////////////////////////////////////////////////////////////////////////////////

static CommSession *s_fake_app_session = (CommSession *)~0;
static uint64_t s_unwritten_value = ~0;

void test_app_session_capabilities__initialize(void) {
  s_app_session_ptr = NULL;
  s_close_called = false;
  s_get_called = false;
  s_open_status = S_SUCCESS;
  s_get_value = 0;
  s_set_value = s_unwritten_value;
  s_has_cache = false;
  s_app_md = (PebbleProcessMd){};
}

void test_app_session_capabilities__cleanup(void) {

}

void test_app_session_capabilities__no_cache_file_and_not_connected(void) {
  s_open_status = E_ERROR;
  bool has_cap =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(false, has_cap);
  cl_assert_equal_b(false, s_get_called);
  cl_assert_equal_b(false, s_close_called);
}

void test_app_session_capabilities__no_cache_file_but_connected(void) {
  s_open_status = E_ERROR;
  s_app_session_ptr = s_fake_app_session;
  bool has_cap =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(true, has_cap);
  cl_assert_equal_b(false, s_get_called);
  cl_assert_equal_b(false, s_close_called);
}

void test_app_session_capabilities__cache_file_and_not_connected(void) {
  s_has_cache = true;
  s_get_value = CommSessionInfiniteLogDumping;
  bool has_cap =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(true, has_cap);
  cl_assert_equal_b(true, s_get_called);
  cl_assert_equal_b(true, s_close_called);
}

void test_app_session_capabilities__cache_file_but_no_key_and_not_connected(void) {
  bool has_cap =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(false, has_cap);
  cl_assert_equal_b(true, s_get_called);
  cl_assert_equal_b(true, s_close_called);
}

void test_app_session_capabilities__cache_file_and_connected_new_value(void) {
  s_has_cache = true;
  s_get_value = CommSessionExtendedNotificationService;
  s_app_session_ptr = s_fake_app_session;

  bool has_cap_infinite_log_dumping =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(true, has_cap_infinite_log_dumping);

  bool has_ext_notifications =
      comm_session_current_app_session_cache_has_capability(CommSessionExtendedNotificationService);
  cl_assert_equal_b(false, has_ext_notifications);

  // Check that cache is re-written:
  cl_assert_equal_i(s_set_value, s_live_capabilities);

  cl_assert_equal_b(true, s_get_called);
  cl_assert_equal_b(true, s_close_called);
}

void test_app_session_capabilities__cache_file_and_connected_same_value(void) {
  s_has_cache = true;
  s_get_value = CommSessionInfiniteLogDumping;
  s_app_session_ptr = s_fake_app_session;

  bool has_cap_infinite_log_dumping =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(true, has_cap_infinite_log_dumping);

  // Check that cache is NOT re-written:
  cl_assert_equal_i(s_set_value, s_unwritten_value);

  cl_assert_equal_b(true, s_get_called);
  cl_assert_equal_b(true, s_close_called);
}
