/* SPDX-FileCopyrightText: 2025 Joshua Jun */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/audio_encoder/audio_encoder_backend.h"

#include "board/board.h"
#include "kernel/pbl_malloc.h"
#include <pbl/drivers/mic.h>
#include <pbl/logging/logging.h>

#include "speex/speex.h"
#include "speex/speex_bits.h"
#include "speex/speex_stereo.h"

#include <inttypes.h>
#include <string.h>

PBL_LOG_MODULE_DECLARE(service_audio_encoder, CONFIG_SERVICE_AUDIO_ENCODER_LOG_LEVEL);

extern const SpeexMode speex_wb_mode;

#define SPEEX_BITSTREAM_VERSION (4)
#define SPEEX_SAMPLE_RATE       (16000) // 16 kHz wideband
#define SPEEX_BIT_RATE          (9800)  // 9.8 kbps
#define SPEEX_QUALITY           (6)     // Quality level (0-10)
#define SPEEX_COMPLEXITY        (1)     // Complexity (1-10, lower for embedded)
#define SPEEX_MAX_PACKET_BYTES  (200)
#define SPEEX_AUDIO_GAIN        (3) // Audio gain multiplier

typedef struct {
  void *enc_state;
  SpeexBits bits;
  SpeexStereoState stereo_state;
  uint32_t frame_size; //!< samples per channel
  uint8_t channels;
  int16_t *work; //!< frame_size * channels samples; encoding modifies its input
  bool open;
} SpeexEncoder;

static SpeexEncoder s_encoder;

static void prv_close(void) {
  if (!s_encoder.open) {
    return;
  }
  if (s_encoder.enc_state) {
    speex_encoder_destroy(s_encoder.enc_state);
  }
  speex_bits_destroy(&s_encoder.bits);
  kernel_free(s_encoder.work);
  s_encoder = (SpeexEncoder){};
}

static bool prv_open(AudioEncoderInfo *info_out) {
  if (s_encoder.open) {
    return false;
  }
  s_encoder = (SpeexEncoder){
    .channels = (uint8_t)mic_get_channels(MIC),
  };

  s_encoder.enc_state = speex_encoder_init(&speex_wb_mode);
  if (!s_encoder.enc_state) {
    PBL_LOG_ERR("Failed to initialize Speex encoder");
    return false;
  }
  speex_bits_init(&s_encoder.bits);
  if (s_encoder.channels == 2) {
    s_encoder.stereo_state = (SpeexStereoState)SPEEX_STEREO_STATE_INIT;
  }

  speex_encoder_ctl(s_encoder.enc_state, SPEEX_GET_FRAME_SIZE, &s_encoder.frame_size);

  int tmp = SPEEX_QUALITY;
  speex_encoder_ctl(s_encoder.enc_state, SPEEX_SET_QUALITY, &tmp);
  tmp = SPEEX_COMPLEXITY;
  speex_encoder_ctl(s_encoder.enc_state, SPEEX_SET_COMPLEXITY, &tmp);
  tmp = SPEEX_SAMPLE_RATE;
  speex_encoder_ctl(s_encoder.enc_state, SPEEX_SET_SAMPLING_RATE, &tmp);
  tmp = SPEEX_BIT_RATE;
  speex_encoder_ctl(s_encoder.enc_state, SPEEX_SET_BITRATE, &tmp);

  int sample_rate = 0;
  int bit_rate = 0;
  speex_encoder_ctl(s_encoder.enc_state, SPEEX_GET_SAMPLING_RATE, &sample_rate);
  speex_encoder_ctl(s_encoder.enc_state, SPEEX_GET_BITRATE, &bit_rate);

  s_encoder.work = kernel_malloc(s_encoder.frame_size * s_encoder.channels * sizeof(int16_t));
  if (!s_encoder.work) {
    speex_encoder_destroy(s_encoder.enc_state);
    speex_bits_destroy(&s_encoder.bits);
    s_encoder = (SpeexEncoder){};
    return false;
  }
  s_encoder.open = true;

  *info_out = (AudioEncoderInfo){
    .codec = AudioCodecSpeexWB,
    .channels = s_encoder.channels,
    .frame_samples = (uint16_t)s_encoder.frame_size,
    .sample_rate = (uint32_t)sample_rate,
    .bitrate = (uint32_t)bit_rate,
    .max_packet_bytes = SPEEX_MAX_PACKET_BYTES,
    .bitstream_version = SPEEX_BITSTREAM_VERSION,
  };
  PBL_LOG_DBG("Speex encoder opened: sample_rate=%d, bit_rate=%d, frame_size=%" PRIu32
              ", channels=%" PRIu8,
              sample_rate, bit_rate, s_encoder.frame_size, s_encoder.channels);
  if (sample_rate != MIC_SAMPLE_RATE) {
    PBL_LOG_WRN("Speex sample rate (%d) != mic sample rate (%d)", sample_rate, MIC_SAMPLE_RATE);
  }
  return true;
}

static int prv_encode(const int16_t *pcm, uint8_t *out, uint32_t out_len) {
  if (!s_encoder.open) {
    return -1;
  }
  const uint32_t total_samples = s_encoder.frame_size * s_encoder.channels;

  // Apply gain, clamped, into the work buffer
  for (uint32_t i = 0; i < total_samples; i++) {
    int32_t boosted = (int32_t)pcm[i] * SPEEX_AUDIO_GAIN;
    if (boosted > INT16_MAX) {
      boosted = INT16_MAX;
    } else if (boosted < INT16_MIN) {
      boosted = INT16_MIN;
    }
    s_encoder.work[i] = (int16_t)boosted;
  }

  speex_bits_reset(&s_encoder.bits);
  if (s_encoder.channels == 2) {
    // Encodes the stereo info and folds the interleaved input to mono in place
    speex_encode_stereo_int(s_encoder.work, s_encoder.frame_size, &s_encoder.bits);
  }
  speex_encode_int(s_encoder.enc_state, (spx_int16_t *)s_encoder.work, &s_encoder.bits);

  const int encoded_bytes = speex_bits_write(&s_encoder.bits, (char *)out, out_len);
  if (encoded_bytes < 0) {
    PBL_LOG_ERR("Failed to write Speex encoded data (%d)", encoded_bytes);
    return -1;
  }
  return encoded_bytes;
}

const AudioEncoderBackend g_audio_encoder_backend_speex = {
  .codec = AudioCodecSpeexWB,
  .open = prv_open,
  .encode = prv_encode,
  .close = prv_close,
};
