/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "call_audio_cvsd.h"

#include <string.h>

// The SDK's codec and filters, built as they are. They use assert() without including it.
#define NDEBUG
#include <assert.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "../../third_party/hal_sifli/SiFli-SDK/middleware/audio/audio_bt_voice/audio_cvsd.c"
#include "../../third_party/hal_sifli/SiFli-SDK/middleware/audio/audio_bt_voice/audio_filter.c"
#pragma GCC diagnostic pop

static_assert(sizeof(struct call_audio_cvsd_codec) == sizeof(cvsd_t), "cvsd_t changed");
static_assert(CALL_AUDIO_CVSD_FIR == FIR_FILTER_LENGTH, "FIR length changed");

// The air carries each byte's bits in the other order than the codec takes them (the SDK's
// Reverse_byte).
static uint8_t prv_reverse(uint8_t b) {
  b = (uint8_t)((b & 0xF0) >> 4 | (b & 0x0F) << 4);
  b = (uint8_t)((b & 0xCC) >> 2 | (b & 0x33) << 2);
  return (uint8_t)((b & 0xAA) >> 1 | (b & 0x55) << 1);
}

void call_audio_cvsd_init(struct call_audio_cvsd *c) {
  memset(c, 0, sizeof(*c));
  cvsdInit((cvsd_t *)&c->dec);
  cvsdInit((cvsd_t *)&c->enc);
}

void call_audio_cvsd_decode(struct call_audio_cvsd *c, const uint8_t air[CALL_AUDIO_CVSD_BYTES],
                            int16_t pcm[CALL_AUDIO_CVSD_SAMPLES]) {
  uint8_t bits[CALL_AUDIO_CVSD_BYTES];
  for (size_t i = 0; i < sizeof(bits); i++) {
    bits[i] = prv_reverse(air[i]);
  }
  // The filter's history: the last samples of the frame before.
  memmove(c->dec_buf, &c->dec_buf[CALL_AUDIO_CVSD_BITS], CALL_AUDIO_CVSD_FIR * sizeof(int16_t));
  cvsdDecode((cvsd_t *)&c->dec, bits, CALL_AUDIO_CVSD_BYTES, &c->dec_buf[CALL_AUDIO_CVSD_FIR]);
  decimation_x8(c->dec_buf, CALL_AUDIO_CVSD_FIR + CALL_AUDIO_CVSD_BITS, pcm,
                CALL_AUDIO_CVSD_SAMPLES);
}

void call_audio_cvsd_encode(struct call_audio_cvsd *c, const int16_t pcm[CALL_AUDIO_CVSD_SAMPLES],
                            uint8_t air[CALL_AUDIO_CVSD_BYTES]) {
  memmove(c->enc_in, &c->enc_in[CALL_AUDIO_CVSD_SAMPLES], CALL_AUDIO_CVSD_FIR * sizeof(int16_t));
  memcpy(&c->enc_in[CALL_AUDIO_CVSD_FIR], pcm, CALL_AUDIO_CVSD_SAMPLES * sizeof(int16_t));
  interpolation_x8(c->enc_in, CALL_AUDIO_CVSD_FIR + CALL_AUDIO_CVSD_SAMPLES, c->enc_up,
                   CALL_AUDIO_CVSD_BITS);
  cvsdEncode((cvsd_t *)&c->enc, c->enc_up, CALL_AUDIO_CVSD_BITS, (uint32_t *)air);
  for (size_t i = 0; i < CALL_AUDIO_CVSD_BYTES; i++) {
    air[i] = prv_reverse(air[i]);
  }
}
