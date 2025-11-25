/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_smartstrap_profiles.h"

#include "clar_asserts.h"

static bool s_did_read;
static bool s_read_success;
static SmartstrapProfile s_read_profile;
static uint32_t s_read_length;

static bool s_did_notify;
static bool s_notify_success;
static bool s_notify_profile;

static bool s_did_request;
static SmartstrapRequest s_request;


void smartstrap_profiles_handle_read(bool success, SmartstrapProfile profile, uint32_t length) {
  s_read_success = success;
  s_read_profile = profile;
  s_read_length = length;
  s_did_read = true;
}

void smartstrap_profiles_handle_read_aborted(SmartstrapProfile profile) {
}

void fake_smartstrap_profiles_check_read_params(bool success, SmartstrapProfile profile,
                                                uint32_t length) {
  cl_assert(s_did_read);
  cl_assert(success == s_read_success);
  cl_assert(profile == s_read_profile);
  cl_assert(length == s_read_length);
  s_did_read = false;
}

void smartstrap_profiles_handle_notification(bool success, SmartstrapProfile profile) {
  s_notify_success = success;
  s_notify_profile = profile;
  s_did_notify = true;
}

void fake_smartstrap_profiles_check_notify_params(bool success, SmartstrapProfile profile) {
  cl_assert(s_did_notify);
  cl_assert(success = s_notify_success);
  cl_assert(profile = s_notify_profile);
  s_did_notify = false;
}

SmartstrapResult smartstrap_profiles_handle_request(const SmartstrapRequest *request) {
  s_request = *request;
  s_did_request = true;
  return SmartstrapResultOk;
}

void fake_smartstrap_profiles_check_request_params(const SmartstrapRequest *request) {
  cl_assert(s_did_request);
  cl_assert(s_request.service_id == request->service_id);
  cl_assert(s_request.attribute_id == request->attribute_id);
  cl_assert((s_request.write_mbuf == NULL) == (request->write_mbuf == NULL));
  cl_assert((s_request.read_mbuf == NULL) == (request->read_mbuf == NULL));
  cl_assert(s_request.timeout_ms == request->timeout_ms);
  s_did_request = false;
}
