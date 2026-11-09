/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <applib/event_service_client.h>
#include <applib/mic_stream.h>

typedef struct MicStreamState {
  EventServiceInfo event_info;
  MicStreamHandlers handlers;
  void *context;
  bool active;
  uint32_t stream_id; //!< Events from other streams are stale
} MicStreamState;

void mic_stream_state_init(MicStreamState *state);
