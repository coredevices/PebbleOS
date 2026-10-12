/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <kernel/pebble_tasks.h>

//! Streams the microphone of the foreground app to the phone.
//!
//! Audio is encoded with dictation's Speex encoder and sent over the audio endpoint in a
//! VoiceEndpointSessionTypeAudioStream session; the phone decodes it and hands it to the app's
//! companion. Only the app task may stream, only while it is in focus, and never from a
//! watchface. The stream stops on any focus loss, when dictation takes the mic, when the phone
//! ends it, or when the app goes away. The OS mic banner is shown while the mic runs.
//!
//! The values of both enums are shared with the applib API.

typedef enum MicStreamServiceStartResult {
  MicStreamServiceStartOk = 0,
  MicStreamServiceStartErrBusy,
  MicStreamServiceStartErrNotForeground,
  MicStreamServiceStartErrInvalidArgs,
  MicStreamServiceStartErrNoMemory,
  MicStreamServiceStartErrWatchface,
  MicStreamServiceStartErrUnavailable,
} MicStreamServiceStartResult;

typedef enum MicStreamServiceStopReason {
  MicStreamServiceStopReasonStopped = 0,
  MicStreamServiceStopReasonFocusLost,
  MicStreamServiceStopReasonPreempted,
  MicStreamServiceStopReasonPhone,
  MicStreamServiceStopReasonError,
} MicStreamServiceStopReason;

void mic_stream_service_init(void);

//! Requests a stream for `owner`. The mic starts once the phone accepts the session, reported
//! with MicStreamEventStarted; a refusal or setup timeout is reported as a Phone stop.
MicStreamServiceStartResult mic_stream_service_start(PebbleTask owner);

//! Stops the stream at the owner's request. No stop event is sent.
void mic_stream_service_stop(PebbleTask owner);

//! Stops the stream because the task is going away. No stop event is sent. Safe when idle.
void mic_stream_service_stop_for_task(PebbleTask task);

//! @return true while streaming, including the setup handshake
bool mic_stream_service_is_active(void);

//! The app lost focus to a modal window.
void mic_stream_service_handle_app_focus_lost(void);

//! Dictation is about to take the mic, the encoder and the phone-side audio session.
void mic_stream_service_handle_system_preempt(void);

//! The phone's answer to the stream session setup. Called by the voice endpoint.
void mic_stream_service_handle_setup_result(uint8_t voice_endpoint_result);
