/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// CVSD on the HCPU, as SiFli's SDK does it on SF32LB52X: the LCPU hands over the air bits of
// each 7.5 ms packet (64 kbit/s, 60 bytes) as they are. Decoding gives 480 samples at 64 kHz,
// filtered down to 60 at 8 kHz; encoding goes the other way. The codec and its filters are the
// SDK's (Synopsys, BSD-3-Clause); the state is kept for the whole call.

#include <stddef.h>
#include <stdint.h>

#define CALL_AUDIO_CVSD_BYTES   60
#define CALL_AUDIO_CVSD_SAMPLES 60
//! Air bits, one per 64 kHz sample.
#define CALL_AUDIO_CVSD_BITS (CALL_AUDIO_CVSD_BYTES * 8)
//! Length of the SDK's FIR filters, kept as history in front of each frame.
#define CALL_AUDIO_CVSD_FIR 64

struct call_audio_cvsd_codec {
  // As the SDK's cvsd_t.
  int accumulator;
  int step_size;
  unsigned int output_byte;
};

//! A call's CVSD state, about 2.3 KB: allocated for the call.
struct call_audio_cvsd {
  struct call_audio_cvsd_codec dec;
  struct call_audio_cvsd_codec enc;
  int16_t dec_buf[CALL_AUDIO_CVSD_FIR + CALL_AUDIO_CVSD_BITS];
  int16_t enc_in[CALL_AUDIO_CVSD_FIR + CALL_AUDIO_CVSD_SAMPLES];
  int16_t enc_up[CALL_AUDIO_CVSD_BITS];
};

void call_audio_cvsd_init(struct call_audio_cvsd *c);

//! One packet's air bits to 60 samples at 8 kHz.
void call_audio_cvsd_decode(struct call_audio_cvsd *c, const uint8_t air[CALL_AUDIO_CVSD_BYTES],
                            int16_t pcm[CALL_AUDIO_CVSD_SAMPLES]);

//! 60 samples at 8 kHz to one packet's air bits; @p air must be 4-byte aligned.
void call_audio_cvsd_encode(struct call_audio_cvsd *c, const int16_t pcm[CALL_AUDIO_CVSD_SAMPLES],
                            uint8_t air[CALL_AUDIO_CVSD_BYTES]);
