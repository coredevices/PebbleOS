/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <kernel/events.h>

/**
 * @defgroup services_music_endpoint Music endpoint
 * @ingroup services
 * @brief Pebble Protocol music backend for @ref services_music.
 *
 * Used with phone apps that do not run on iOS, where the Apple Media Service is used instead.
 * Commands go to the phone over the system session on endpoint 0x20; responses update the music
 * service.
 * @{
 */

/**
 * @brief Handle the mobile app info event.
 *
 * Connects this backend, disconnecting AMS, when the phone app runs on Android, macOS, Linux or
 * Windows.
 *
 * @param app_info_event Event reported by the phone app.
 */
void music_endpoint_handle_mobile_app_info_event(const PebbleRemoteAppInfoEvent *app_info_event);

/**
 * @brief Handle a comm session event.
 *
 * Disconnects this backend when the system session closes.
 *
 * @param app_event Session event.
 */
void music_endpoint_handle_mobile_app_event(const PebbleCommSessionEvent *app_event);

/** @} */
