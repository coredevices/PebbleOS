/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! Currently this file serves as a cache for the existence of the SEND_TEXT_NOTIF_PREF_KEY, and
//! a reply action within that key.
//! This is required because a user can have a supported mobile app but not a supported carrier,
//! and in that case we don't want to show the app in the launcher.
//! We cache the existence of this key so that the launcher isn't slowed down by flash reads

void send_text_service_init(void);

bool send_text_service_is_send_text_supported(void);
