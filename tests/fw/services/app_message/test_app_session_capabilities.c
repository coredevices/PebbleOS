/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/services/comm_session/app_session_capabilities.h>
#include <pbl/services/comm_session/session.h>
#include <pbl/settings/settings.h>

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
FAKE_VALUE_FUNC(int, pbl_settings_open, struct pbl_settings_file *, const char *, int);
FAKE_VOID_FUNC(pbl_settings_close, struct pbl_settings_file *);
FAKE_VALUE_FUNC(int, pbl_settings_get, struct pbl_settings_file *, const void *, size_t, void *,
                size_t);
FAKE_VALUE_FUNC(int, pbl_settings_set, struct pbl_settings_file *, const void *, size_t,
                const void *, size_t);
FAKE_VALUE_FUNC(int, pbl_settings_delete, struct pbl_settings_file *, const void *, size_t);
FAKE_VALUE_FUNC(int, pbl_settings_rewrite, struct pbl_settings_file *, pbl_settings_rewrite_cb_t,
                void *);

#define FAKES(FAKE)                          \
  FAKE(comm_session_get_current_app_session) \
  FAKE(pbl_settings_open)                    \
  FAKE(pbl_settings_close)                   \
  FAKE(pbl_settings_get)                     \
  FAKE(pbl_settings_set)                     \
  FAKE(pbl_settings_delete)                  \
  FAKE(pbl_settings_rewrite)

static uint64_t s_get_value;
static int prv_settings_file_get(struct pbl_settings_file *file, const void *key, size_t key_len,
                                 void *val_out, size_t val_out_len) {
  *((uint64_t *)val_out) = s_get_value;
  return 0;
}

static uint64_t s_set_value;
static int prv_settings_file_set(struct pbl_settings_file *file, const void *key, size_t key_len,
                                 const void *val, size_t val_len) {
  cl_assert_equal_i(val_len, sizeof(uint64_t));
  s_set_value = *(uint64_t *)val;
  return 0;
}

static void prv_set_cache(uint64_t value) {
  s_get_value = value;
  pbl_settings_get_fake.custom_fake = prv_settings_file_get;
}

// Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////

// Tests
////////////////////////////////////////////////////////////////////////////////////////////////////

static CommSession *s_fake_app_session = (CommSession *)~0;

void test_app_session_capabilities__initialize(void) {
  FAKES(RESET_FAKE);
  FFF_RESET_HISTORY();
  pbl_settings_get_fake.return_val = -ENOENT;
  pbl_settings_set_fake.custom_fake = prv_settings_file_set;
  s_app_md = (PebbleProcessMd){};
}

void test_app_session_capabilities__cleanup(void) {
}

void test_app_session_capabilities__no_cache_file_and_not_connected(void) {
  pbl_settings_open_fake.return_val = E_ERROR;
  bool has_cap =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(false, has_cap);
  cl_assert_equal_i(pbl_settings_get_fake.call_count, 0);
  cl_assert_equal_i(pbl_settings_close_fake.call_count, 0);
}

void test_app_session_capabilities__no_cache_file_but_connected(void) {
  pbl_settings_open_fake.return_val = E_ERROR;
  comm_session_get_current_app_session_fake.return_val = s_fake_app_session;
  bool has_cap =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(true, has_cap);
  cl_assert_equal_i(pbl_settings_get_fake.call_count, 0);
  cl_assert_equal_i(pbl_settings_close_fake.call_count, 0);
}

void test_app_session_capabilities__cache_file_and_not_connected(void) {
  prv_set_cache(CommSessionInfiniteLogDumping);
  bool has_cap =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(true, has_cap);
  cl_assert_equal_i(pbl_settings_get_fake.call_count, 1);
  cl_assert_equal_i(pbl_settings_close_fake.call_count, 1);
}

void test_app_session_capabilities__cache_file_but_no_key_and_not_connected(void) {
  bool has_cap =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(false, has_cap);
  cl_assert_equal_i(pbl_settings_get_fake.call_count, 1);
  cl_assert_equal_i(pbl_settings_close_fake.call_count, 1);
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
  cl_assert(pbl_settings_set_fake.call_count > 0);
  cl_assert_equal_i(s_set_value, s_live_capabilities);

  cl_assert_equal_i(pbl_settings_get_fake.call_count, 2);
  cl_assert_equal_i(pbl_settings_close_fake.call_count, 2);
}

void test_app_session_capabilities__cache_file_and_connected_same_value(void) {
  prv_set_cache(CommSessionInfiniteLogDumping);
  comm_session_get_current_app_session_fake.return_val = s_fake_app_session;

  bool has_cap_infinite_log_dumping =
      comm_session_current_app_session_cache_has_capability(CommSessionInfiniteLogDumping);
  cl_assert_equal_b(true, has_cap_infinite_log_dumping);

  // Check that cache is NOT re-written:
  cl_assert_equal_i(pbl_settings_set_fake.call_count, 0);

  cl_assert_equal_i(pbl_settings_get_fake.call_count, 1);
  cl_assert_equal_i(pbl_settings_close_fake.call_count, 1);
}
