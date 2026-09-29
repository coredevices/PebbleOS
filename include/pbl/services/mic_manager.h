/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/mic.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//! Arbitrates the single microphone between kernel clients.
//!
//! Dictation (the system voice UI) has priority over app capture: acquiring for dictation while an
//! app owns the mic preempts the app, which is told through its preempted callback before the mic
//! is restarted. An app cannot acquire the mic while dictation owns it.

typedef enum MicClient {
  MicClientNone = 0,
  MicClientVoiceDictation,
  MicClientAppCapture,
} MicClient;

//! Called (without the manager lock held) when the client's mic is taken away. The client must
//! not call mic_manager_release() for the lost session; it is already released.
typedef void (*MicManagerPreemptedCb)(void *context);

void mic_manager_init(void);

//! Starts the microphone for a client. See \ref mic_start for the buffer semantics.
//! @return true if the client now owns the running microphone
bool mic_manager_acquire(MicClient client, MicDataHandlerCB handler, void *context, int16_t *buffer,
                         size_t buffer_len, MicManagerPreemptedCb on_preempted,
                         void *preempt_context);

//! Stops the microphone if the client owns it. No-op otherwise.
void mic_manager_release(MicClient client);

MicClient mic_manager_get_owner(void);
