/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//! @addtogroup Microphone
//! @{
//! Local 16 kHz mono signed 16-bit PCM capture. Requires an explicit Microphone
//! grant synced from the phone. Grants persist offline until updated by the phone.
//! Only foreground watchapps can capture. The system displays its Listening banner.
#define MIC_CAPTURE_SAMPLE_RATE 16000

typedef enum MicCaptureStartResult {
  MicCaptureStartOk = 0,
  MicCaptureStartErrBusy,
  MicCaptureStartErrNotForeground,
  MicCaptureStartErrInvalidArgs,
  MicCaptureStartErrWatchface,
  MicCaptureStartErrUnavailable,
  MicCaptureStartErrPermissionDenied,
} MicCaptureStartResult;

typedef enum MicCaptureStopReason {
  MicCaptureStopReasonFocusLost = 0,
  MicCaptureStopReasonInterrupted,
  MicCaptureStopReasonPermissionRevoked,
} MicCaptureStopReason;

//! Runs on the app task. Samples are valid only during this callback. Slow consumers
//! drop frames; each callback receives 320 samples (20 ms).
typedef void (*MicCaptureDataHandler)(const int16_t *samples, size_t sample_count, void *context);
//! Called on the app task when capture ends involuntarily.
typedef void (*MicCaptureStoppedHandler)(MicCaptureStopReason reason, void *context);

typedef struct MicCaptureHandlers {
  MicCaptureDataHandler data;
  MicCaptureStoppedHandler stopped;
} MicCaptureHandlers;

//! Both handlers are required. Capture starts immediately on success.
MicCaptureStartResult mic_capture_start(MicCaptureHandlers handlers, void *context);
//! Stops capture without calling stopped. Safe while idle.
void mic_capture_stop(void);
bool mic_capture_is_active(void);
//! @}
