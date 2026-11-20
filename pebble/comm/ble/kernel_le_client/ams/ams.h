/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! @file ams.h The session with the Apple Media Service of the phone: it registers for the
//! player, the queue and the track with the AMS client, and is the music server the music
//! service controls the phone through. KernelMain only.

//! Starts a session on the connection of the phone: looks for the service, and subscribes to
//! it when the phone has it.
void ams_create(uint16_t conn_handle);

//! Ends the session.
void ams_destroy(void);

//! The services of the phone changed: looks for the service again.
void ams_handle_services_changed(void);

//! Disconnect AMS from the music service if currently registered. No-op otherwise.
//! Some Android phones expose something that looks like an AMS GATT service,
//! letting AMS grab the music slot before we learn the remote is Android. The PP
//! music endpoint calls this on Android-detect so it can take over the slot.
void ams_music_disconnect(void);

//! For testing purposes.
//! @return The debug name with which AMS registers itself with the music.c service.
const char *ams_music_server_debug_name(void);

//! For testing purposes.
//! @return Whether AMS has registered itself for updates of all entities (Player, Queue and Track).
bool ams_is_registered_for_all_entity_updates(void);
