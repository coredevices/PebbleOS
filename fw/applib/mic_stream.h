/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup Microphone
//! \brief Stream the watch microphone to the phone
//!
//! Streams the microphone to your app's PebbleKit JS companion. The watch encodes the audio
//! (Speex wideband, ~10 kbps) and the phone decodes it and delivers 16 kHz, 16-bit mono PCM to
//! your JavaScript as `audiostreamstart`, `audiostream` and `audiostreamstop` events.
//!
//! It is for watchapps only: a watchface can never stream. While the microphone runs, the system
//! shows a "Listening" banner at the bottom of the screen which reduces your unobstructed area
//! (see \ref layer_get_unobstructed_bounds). The phone decides whether to accept a stream and can
//! end it at any time. The stream also ends on its own, with a \ref MicStreamStopReason, whenever
//! your app loses focus (a notification, alert, or the dictation UI) or the system needs the
//! microphone.
//!   @{

//! Result of \ref mic_stream_to_phone_start.
typedef enum MicStreamStartResult {
  //! The stream was requested; `started` is called once the phone accepts it
  MicStreamStartOk = 0,
  //! The microphone is in use (by this app or the system)
  MicStreamStartErrBusy,
  //! The app is not in the foreground
  MicStreamStartErrNotForeground,
  //! Invalid arguments
  MicStreamStartErrInvalidArgs,
  //! Not enough memory to start the stream
  MicStreamStartErrNoMemory,
  //! Watchfaces can never stream
  MicStreamStartErrWatchface,
  //! This watch has no microphone
  MicStreamStartErrUnavailable,
} MicStreamStartResult;

//! Why the stream ended.
typedef enum MicStreamStopReason {
  //! The app stopped the stream
  MicStreamStopReasonStopped = 0,
  //! The app lost focus, e.g. a notification appeared
  MicStreamStopReasonFocusLost,
  //! The system took the microphone, e.g. for dictation
  MicStreamStopReasonInterrupted,
  //! The phone refused, stopped or lost the stream
  MicStreamStopReasonPhone,
  //! An unexpected error
  MicStreamStopReasonError,
} MicStreamStopReason;

//! Handler called once the phone has accepted the stream and audio is flowing.
typedef void (*MicStreamStartedHandler)(void *context);

//! Handler called when the stream ends for a reason other than the app stopping it. The stream
//! is gone by the time it is called.
typedef void (*MicStreamStoppedHandler)(MicStreamStopReason reason, void *context);

typedef struct MicStreamHandlers {
  MicStreamStartedHandler started;
  MicStreamStoppedHandler stopped;
} MicStreamHandlers;

//! Starts streaming the microphone to the app's PebbleKit JS companion.
//! @param handlers `stopped` is required
//! @param context Passed to the handlers
MicStreamStartResult mic_stream_to_phone_start(MicStreamHandlers handlers, void *context);

//! Stops streaming. Safe to call when not streaming.
void mic_stream_to_phone_stop(void);

//! @return true while the app is streaming to the phone, including while the phone has yet to
//! accept the stream
bool mic_stream_to_phone_is_active(void);

//!   @} // end addtogroup Microphone
//! @} // end addtogroup Foundation
