/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/audio_encoder.h"

#include "syscall/syscall.h"

bool audio_encoder_codec_available(AudioCodec codec) {
  return sys_audio_encoder_codec_available((uint8_t)codec);
}

bool audio_encoder_open(AudioCodec codec, AudioEncoderInfo *info_out) {
  if (!info_out) {
    return false;
  }
  return sys_audio_encoder_open((uint8_t)codec, info_out);
}

int audio_encoder_encode_frame(const int16_t *pcm, uint32_t num_samples, uint8_t *out,
                               uint32_t out_len) {
  if (!pcm || !out) {
    return -1;
  }
  return sys_audio_encoder_encode(pcm, num_samples, out, out_len);
}

void audio_encoder_close(void) {
  sys_audio_encoder_close();
}
