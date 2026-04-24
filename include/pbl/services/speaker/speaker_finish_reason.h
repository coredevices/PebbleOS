/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Reason reported when speaker playback ends.
typedef enum {
  SpeakerFinishReasonDone = 0,       //!< Playback completed naturally
  SpeakerFinishReasonStopped,        //!< Playback was stopped by the app
  SpeakerFinishReasonPreempted,      //!< Preempted by higher priority source
  SpeakerFinishReasonError,          //!< An error occurred
} SpeakerFinishReason;
