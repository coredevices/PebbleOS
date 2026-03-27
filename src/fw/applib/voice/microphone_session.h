/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! @file voice/microphone_session.h
//! Raw microphone audio streaming API.
//!
//! A MicrophoneSession streams Speex-encoded audio from the watch microphone to the connected
//! phone app without performing speech-to-text. The phone app receives the raw Speex frames via
//! the audio endpoint protocol and is responsible for decoding and processing them.
//!
//! Usage:
//!   1. Call microphone_session_create() to allocate a session.
//!   2. Call microphone_session_start() to begin streaming; your callback fires when the phone
//!      is ready (MicrophoneSessionStatusRecording) or if setup fails.
//!   3. Call microphone_session_stop() when done; your callback fires with
//!      MicrophoneSessionStatusSuccess when the stream has been flushed.
//!   4. Call microphone_session_destroy() to free the session.
//!
//! Only one audio session (dictation or raw audio) may be active at a time.

typedef struct MicrophoneSession MicrophoneSession;

typedef enum {
  //! Streaming started — the phone is receiving Speex frames.
  MicrophoneSessionStatusRecording,

  //! Streaming stopped cleanly after microphone_session_stop().
  MicrophoneSessionStatusSuccess,

  //! No BT or internet connection to the phone app.
  MicrophoneSessionStatusFailureConnectivityError,

  //! Raw audio streaming is disabled on this platform or by the phone app.
  MicrophoneSessionStatusFailureDisabled,

  //! An internal error prevented the session from starting or continuing.
  MicrophoneSessionStatusFailureInternalError,
} MicrophoneSessionStatus;

//! Callback invoked when the session state changes.
//! @param session  The session that generated the event.
//! @param status   The new session status.
//! @param context  The context pointer supplied to microphone_session_create().
typedef void (*MicrophoneSessionStatusCallback)(MicrophoneSession *session,
                                                MicrophoneSessionStatus status,
                                                void *context);

//! Allocate a microphone session.
//! @param callback          Status callback — must not be NULL.
//! @param callback_context  Opaque pointer forwarded to every callback invocation.
//! @return Pointer to the new session, or NULL if the platform lacks a microphone, the phone
//!         app does not support raw audio streaming, or an internal allocation failure occurred.
MicrophoneSession *microphone_session_create(MicrophoneSessionStatusCallback callback,
                                             void *callback_context);

//! Free a microphone session. If a stream is in progress it is cancelled first.
//! @param session  Session to destroy; a NULL pointer is silently ignored.
void microphone_session_destroy(MicrophoneSession *session);

//! Begin streaming audio to the phone app.
//! Can only be called when no stream is already in progress.
//! @param session  Session returned by microphone_session_create().
//! @return MicrophoneSessionStatusRecording on success, or a failure status.
MicrophoneSessionStatus microphone_session_start(MicrophoneSession *session);

//! Stop an in-progress audio stream.
//! The callback will fire with MicrophoneSessionStatusSuccess once the endpoint has been flushed.
//! @param session  Session returned by microphone_session_create().
//! @return MicrophoneSessionStatusSuccess, or MicrophoneSessionStatusFailureInternalError if no
//!         stream was in progress.
MicrophoneSessionStatus microphone_session_stop(MicrophoneSession *session);
