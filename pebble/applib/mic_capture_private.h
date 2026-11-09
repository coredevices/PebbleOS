/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <applib/event_service_client.h>
#include <applib/mic_capture.h>

typedef struct MicCaptureState {
  EventServiceInfo event_info;
  MicCaptureHandlers handlers;
  void *context;
  uint32_t session;
  int16_t *frame; //!< One frame on the app heap, too large for the app stack
} MicCaptureState;

void mic_capture_state_init(MicCaptureState *state);
