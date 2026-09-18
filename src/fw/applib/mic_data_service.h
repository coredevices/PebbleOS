/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup Microphone
//! \brief Live PCM audio from the watch microphone
//!
//! The Microphone API delivers raw 16 kHz, 16-bit mono PCM to your app while it is in the
//! foreground. It is for watchapps only: a watchface is never allowed to record, whatever its
//! manifest declares. To use it, declare `capabilities: ["microphone"]` in your app's manifest; the
//! user is asked to grant the permission in the phone app and can revoke it at any time (see the
//! Permissions API). While your app captures, the system shows a "Listening" banner at the bottom
//! of the screen which reduces your unobstructed area (see \ref layer_get_unobstructed_bounds).
//!
//! Capture ends on its own, with a \ref MicDataStopReason, whenever your app loses focus (a
//! notification, alert, or the dictation UI), the permission is revoked, or the system needs the
//! microphone. Your app owns everything after capture: processing, encoding, buffering and
//! delivery to the phone.
//!   @{

//! Sample rate of the delivered audio, in Hz.
#define MIC_DATA_SAMPLE_RATE (16000)

//! Result of \ref mic_data_service_subscribe.
typedef enum MicDataStartResult {
  //! Capture started
  MicDataStartOk = 0,
  //! The app does not declare the microphone capability in its manifest
  MicDataStartErrNotDeclared,
  //! The user has not granted the microphone permission
  MicDataStartErrPermissionDenied,
  //! The microphone is in use (by this app or the system)
  MicDataStartErrBusy,
  //! The app is not in the foreground
  MicDataStartErrNotForeground,
  //! Invalid arguments (see the limits on samples_per_update)
  MicDataStartErrInvalidArgs,
  //! Not enough memory to start capture
  MicDataStartErrNoMemory,
  //! Watchfaces can never record
  MicDataStartErrWatchface,
} MicDataStartResult;

//! Why capture ended.
typedef enum MicDataStopReason {
  //! The app unsubscribed
  MicDataStopReasonStopped = 0,
  //! The app lost focus, e.g. a notification appeared
  MicDataStopReasonFocusLost,
  //! The system took the microphone, e.g. for dictation
  MicDataStopReasonInterrupted,
  //! The user revoked the microphone permission
  MicDataStopReasonPermissionRevoked,
  //! An unexpected error
  MicDataStopReasonError,
  //! The phone refused, stopped or lost the stream (streaming only)
  MicDataStopReasonPhone,
} MicDataStopReason;

//! Handler receiving a batch of samples.
//! @param samples samples_per_update signed 16-bit mono samples. Only valid during the call.
//! @param num_samples Number of samples in the batch
//! @param overrun true if samples were dropped before this batch because the app fell behind
//! @param context The context passed to \ref mic_data_service_subscribe
typedef void (*MicDataHandler)(const int16_t *samples, uint32_t num_samples, bool overrun,
                               void *context);

//! Handler called when capture ends for a reason other than the app unsubscribing. The
//! subscription is gone by the time it is called.
typedef void (*MicDataStoppedHandler)(MicDataStopReason reason, void *context);

typedef struct MicDataHandlers {
  MicDataHandler data;
  MicDataStoppedHandler stopped;
} MicDataHandlers;

//! Minimum number of samples per update (5 ms).
#define MIC_DATA_MIN_SAMPLES_PER_UPDATE (80)
//! Maximum number of samples per update (100 ms).
#define MIC_DATA_MAX_SAMPLES_PER_UPDATE (1600)

//! Starts capturing and subscribes to batches of samples.
//! @param samples_per_update Batch size, between \ref MIC_DATA_MIN_SAMPLES_PER_UPDATE and
//! \ref MIC_DATA_MAX_SAMPLES_PER_UPDATE. Choose your encoder's frame size.
//! @param handlers The handlers to call; `data` is required
//! @param context User-provided context passed to the handlers
MicDataStartResult mic_data_service_subscribe(uint32_t samples_per_update, MicDataHandlers handlers,
                                              void *context);

//! Stops capturing. Safe to call when not capturing.
void mic_data_service_unsubscribe(void);

//! @return true while the app is capturing or streaming
bool mic_data_service_is_active(void);

//! Handler called once the phone has accepted the stream and audio is flowing.
typedef void (*MicStreamStartedHandler)(void *context);

typedef struct MicStreamHandlers {
  MicStreamStartedHandler started;
  MicDataStoppedHandler stopped;
} MicStreamHandlers;

//! Streams the microphone straight to the phone, encoded (Speex wideband, ~10 kbps), instead of
//! delivering samples to the app. The phone decodes it and hands the PCM to the app's
//! companion (PebbleKit JS `audiostream` events). The same rules as
//! \ref mic_data_service_subscribe apply; in addition the phone has to accept the stream, which
//! is reported through `started`, and can end it, reported as \ref MicDataStopReasonPhone.
//! @param handlers `stopped` is required
//! @param context Passed to the handlers
MicDataStartResult mic_stream_to_phone_start(MicStreamHandlers handlers, void *context);

//! Stops streaming. Safe to call when not streaming.
void mic_stream_to_phone_stop(void);

//! @return true while the app is streaming to the phone (including the setup handshake)
bool mic_stream_to_phone_is_active(void);

//!   @} // end addtogroup Microphone
//! @} // end addtogroup Foundation
