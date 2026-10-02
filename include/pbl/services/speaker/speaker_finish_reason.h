/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/** @brief Reason reported when speaker playback ends. */
typedef enum {
  /** Playback completed naturally */
  SpeakerFinishReasonDone = 0,
  /** Playback was stopped by the app */
  SpeakerFinishReasonStopped,
  /** Preempted by higher priority source */
  SpeakerFinishReasonPreempted,
  /** An error occurred */
  SpeakerFinishReasonError,
} SpeakerFinishReason;
