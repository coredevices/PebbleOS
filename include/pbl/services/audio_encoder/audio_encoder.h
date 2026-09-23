/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/pebble_tasks.h"
#include "pbl/services/audio_encoder/audio_encoder_types.h"

#include <stdbool.h>
#include <stdint.h>

//! Codec-agnostic speech encoder shared by dictation and the app Microphone API.
//!
//! One encoder instance exists at a time, owned by a task. Dictation opens it as the system
//! owner and takes it away from an app if needed (the app's capture has already been preempted
//! by then). Backends are selected at build time; see audio_encoder_backend.h.

_Static_assert(sizeof(AudioEncoderInfo) == 16, "AudioEncoderInfo is part of the SDK ABI");

//! Largest input frame any backend accepts, in samples (all channels).
#define AUDIO_ENCODER_MAX_FRAME_SAMPLES (1600)
//! Largest encoded packet any backend produces.
#define AUDIO_ENCODER_MAX_PACKET_BYTES (512)

//! Owner used by kernel clients (dictation).
#define AUDIO_ENCODER_SYSTEM_OWNER (PebbleTask_KernelMain)

void audio_encoder_service_init(void);

bool audio_encoder_service_is_codec_available(AudioCodec codec);

//! Opens the encoder for `owner`. Fails if another task holds it, unless the caller is the
//! system owner, which closes an app's encoder first.
//! @param codec Codec to encode with
//! @param owner Task that will own the encoder
//! @param[out] info_out Filled on success
bool audio_encoder_service_open(AudioCodec codec, PebbleTask owner, AudioEncoderInfo *info_out);

//! Encodes exactly frame_samples * channels samples.
//! @return number of bytes written to out, or a negative value on error
int audio_encoder_service_encode(PebbleTask owner, const int16_t *pcm, uint32_t num_samples,
                                 uint8_t *out, uint32_t out_len);

//! Closes the encoder if `owner` holds it.
void audio_encoder_service_close(PebbleTask owner);

//! Closes the encoder if `task` holds it. For process cleanup.
void audio_encoder_service_close_for_task(PebbleTask task);

bool audio_encoder_service_is_open(void);
