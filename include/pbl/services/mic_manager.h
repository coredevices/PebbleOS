/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/pebble_tasks.h>

//! Arbitrates the single microphone between kernel clients.
//!
//! Dictation (the system voice UI) has priority over app capture: acquiring for dictation while an
//! app owns the mic preempts the app, which is told through its preempted callback before the mic
//! is restarted. An app cannot acquire the mic while dictation owns it. The OS mic banner is shown
//! while an app client owns the running mic.

//! Sample rate of the frames clients receive, in Hz.
#define MIC_MANAGER_SAMPLE_RATE 16000

//! Channels of the frames clients receive.
#define MIC_MANAGER_CHANNELS 1

//! Handler for a full frame of captured audio, called on KernelBG.
//! @param samples Captured samples, the buffer passed when acquiring.
//! @param sample_count Number of 16-bit samples in @p samples.
//! @param context User context passed when acquiring.
typedef void (*MicDataHandlerCB)(int16_t *samples, size_t sample_count, void *context);

typedef enum MicClient {
  MicClientNone = 0,
  MicClientVoiceDictation,
  MicClientAppStream,
  MicClientAppCapture,
} MicClient;

typedef enum MicManagerAppCheck {
  MicManagerAppOk = 0,
  MicManagerAppNotForeground,
  MicManagerAppWatchface,
} MicManagerAppCheck;

//! Whether `task` is a foreground app in focus, the only kind that may use the mic.
MicManagerAppCheck mic_manager_check_app(PebbleTask task);

//! Called (without the manager lock held) when the client's mic is taken away. The client must
//! not call mic_manager_release() for the lost session; it is already released.
typedef void (*MicManagerPreemptedCb)(void *context);

void mic_manager_init(void);

//! Starts the microphone for a client, which receives frames of @p buffer_len samples in
//! @p buffer. The buffer is owned by the manager until the client releases the mic or is
//! preempted.
//! @return true if the client now owns the running microphone
bool mic_manager_acquire(MicClient client, MicDataHandlerCB handler, void *context, int16_t *buffer,
                         size_t buffer_len, MicManagerPreemptedCb on_preempted,
                         void *preempt_context);

//! Holds the idle mic for an app client whose start is pending, so other apps see it as busy.
//! Only that client or dictation may then acquire it. Cleared by acquire or release.
//! @return true if the mic was idle and unreserved
bool mic_manager_reserve(MicClient client);

//! Stops the microphone if the client owns it and drops its reservation. No-op otherwise.
void mic_manager_release(MicClient client);

//! Set for a whole dictation session, from before it takes the audio endpoint until it ends. Apps
//! can neither reserve nor acquire the mic meanwhile.
void mic_manager_set_dictation_active(bool active);

MicClient mic_manager_get_owner(void);

//! @return true while the mic is owned or reserved, or a dictation session runs
bool mic_manager_is_busy(void);
