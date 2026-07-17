/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "event_service_client.h"
#include "music_service.h"

typedef struct __attribute__((packed)) MusicServiceState {
  MusicServiceEventHandler handler;

  EventServiceInfo mss_info;
} MusicServiceState;

void music_service_state_init(MusicServiceState *state);
