/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/accessory/smartstrap_comms.h"

#include <stdbool.h>
#include <stdint.h>

void smartstrap_profiles_handle_read(bool success, SmartstrapProfile profile, uint32_t length);

void fake_smartstrap_profiles_check_read_params(bool success, SmartstrapProfile profile,
                                                uint32_t length);

void smartstrap_profiles_handle_notification(bool success, SmartstrapProfile profile);

void fake_smartstrap_profiles_check_notify_params(bool success, SmartstrapProfile profile);

SmartstrapResult smartstrap_profiles_handle_request(const SmartstrapRequest *request);

void smartstrap_profiles_handle_read_aborted(SmartstrapProfile profile);

void fake_smartstrap_profiles_check_request_params(const SmartstrapRequest *request);
