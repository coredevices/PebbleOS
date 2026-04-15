/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"

void music_endpoint_handle_mobile_app_info_event(const PebbleRemoteAppInfoEvent *app_info_event);

void music_endpoint_handle_mobile_app_event(const PebbleCommSessionEvent *app_event);
