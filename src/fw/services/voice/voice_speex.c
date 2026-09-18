/* SPDX-FileCopyrightText: 2025 Joshua Jun */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/voice/voice_speex.h"

#include "kernel/pbl_malloc.h"
#include "pbl/services/audio_encoder/audio_encoder.h"
#include "system/passert.h"
#include <pbl/logging/logging.h>

#include <inttypes.h>
#include <string.h>

PBL_LOG_MODULE_DECLARE(service_voice, CONFIG_SERVICE_VOICE_LOG_LEVEL);

// Dictation's view of the shared audio encoder: the Speex backend opened as the system owner,
// plus the buffer the mic driver fills with one frame at a time.
typedef struct {
  AudioEncoderInfo info;
  int16_t *frame_buffer;
  bool initialized;
} VoiceSpeexState;

static VoiceSpeexState s_speex;

static size_t prv_frame_buffer_size(void) {
  return (size_t)s_speex.info.frame_samples * s_speex.info.channels * sizeof(int16_t);
}

bool voice_speex_init(void) {
  if (s_speex.initialized) {
    return true;
  }
  if (!audio_encoder_service_open(AudioCodecSpeexWB, AUDIO_ENCODER_SYSTEM_OWNER, &s_speex.info)) {
    PBL_LOG_ERR("Failed to open Speex encoder");
    return false;
  }
  s_speex.frame_buffer = kernel_malloc(prv_frame_buffer_size());
  if (!s_speex.frame_buffer) {
    audio_encoder_service_close(AUDIO_ENCODER_SYSTEM_OWNER);
    return false;
  }
  s_speex.initialized = true;
  return true;
}

void voice_speex_deinit(void) {
  if (!s_speex.initialized) {
    return;
  }
  audio_encoder_service_close(AUDIO_ENCODER_SYSTEM_OWNER);
  kernel_free(s_speex.frame_buffer);
  s_speex = (VoiceSpeexState){};
}

void voice_speex_get_transfer_info(AudioTransferInfoSpeex *info) {
  PBL_ASSERTN(s_speex.initialized);
  PBL_ASSERTN(info);

  memset(info, 0, sizeof(AudioTransferInfoSpeex));
  strncpy(info->version, "1.2.1", sizeof(info->version) - 1);
  info->sample_rate = s_speex.info.sample_rate;
  info->bit_rate = (uint16_t)s_speex.info.bitrate;
  info->frame_size = s_speex.info.frame_samples;
  info->bitstream_version = s_speex.info.bitstream_version;

  PBL_LOG_DBG("Transfer info: sample_rate=%" PRIu32 ", bit_rate=%" PRIu16 ", frame_size=%" PRIu16
              ", bitstream_version=%" PRIu8,
              info->sample_rate, info->bit_rate, info->frame_size, info->bitstream_version);
}

int voice_speex_get_frame_size(void) {
  return s_speex.initialized ? (int)(s_speex.info.frame_samples * s_speex.info.channels) : 0;
}

int16_t *voice_speex_get_frame_buffer(void) {
  return s_speex.initialized ? s_speex.frame_buffer : NULL;
}

size_t voice_speex_get_frame_buffer_size(void) {
  return s_speex.initialized ? prv_frame_buffer_size() : 0;
}

int voice_speex_encode_frame(int16_t *samples, uint8_t *encoded_data, size_t max_encoded_size) {
  if (!s_speex.initialized) {
    PBL_LOG_ERR("encode_frame called but Speex not initialized");
    return -1;
  }
  return audio_encoder_service_encode(AUDIO_ENCODER_SYSTEM_OWNER, samples,
                                      voice_speex_get_frame_size(), encoded_data, max_encoded_size);
}

bool voice_speex_is_initialized(void) {
  return s_speex.initialized;
}
