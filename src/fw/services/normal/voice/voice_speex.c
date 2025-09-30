/*
 * Copyright 2025 Joshua Jun
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "voice_speex.h"

#include "system/logging.h"
#include "system/passert.h"
#include "kernel/pbl_malloc.h"
#include "system/logging.h"
#include "services/normal/audio_endpoint.h"
#include "drivers/mic.h"

#include "speex/speex.h"
#include "speex/speex_bits.h"
#include "speex/speex_header.h"

#include <string.h>
#include <inttypes.h>

// External Speex mode declarations
extern const SpeexMode speex_wb_mode;

#define VOICE_SPEEX_LOG(fmt, args...) PBL_LOG_D(LOG_DOMAIN_VOICE, LOG_LEVEL_DEBUG, fmt, ## args)

// Speex bitstream version
#define SPEEX_BITSTREAM_VERSION 4

// Speex encoder state
typedef struct {
  void *enc_state;
  SpeexBits bits;
  uint32_t frame_size;       // Changed to uint32_t to match transfer info
  uint32_t sample_rate;      // Changed to uint32_t to match transfer info
  uint16_t bit_rate;         // Changed to uint16_t to match transfer info
  uint8_t bitstream_version; // Changed to uint8_t to match transfer info
  bool initialized;
  uint8_t *frame_buffer;
  size_t frame_buffer_size;
  uint8_t *encoded_buffer;
  size_t encoded_buffer_size;
} VoiceSpeexEncoder;

static VoiceSpeexEncoder s_encoder = {0};

// Speex configuration
#define SPEEX_SAMPLE_RATE 16000  // 16 kHz wideband
#define SPEEX_BIT_RATE 9800      // 9.8 kbps
#define SPEEX_QUALITY 6          // Quality level (0-10)
#define SPEEX_COMPLEXITY 1       // Complexity (1-10, lower for embedded)
#define SPEEX_ENCODED_BUFFER_SIZE 320  // Max encoded frame size
#define SPEEX_AUDIO_GAIN 3       // Audio gain multiplier (3x)

bool voice_speex_init(void) {
  if (s_encoder.initialized) {
    return true;
  }

  memset(&s_encoder, 0, sizeof(s_encoder));

  // Initialize Speex encoder - use wideband mode for 16kHz sample rate
  const SpeexMode *mode = &speex_wb_mode;
  if (!mode) {
    VOICE_SPEEX_LOG("ERROR: Failed to get Speex wideband mode");
    return false;
  }
  
  s_encoder.enc_state = speex_encoder_init(mode);
  if (!s_encoder.enc_state) {
    VOICE_SPEEX_LOG("ERROR: Failed to initialize Speex encoder");
    return false;
  }

  // Initialize bits structure
  speex_bits_init(&s_encoder.bits);

  // Get frame size
  speex_encoder_ctl(s_encoder.enc_state, SPEEX_GET_FRAME_SIZE, &s_encoder.frame_size);
  VOICE_SPEEX_LOG("Initial frame size from Speex: %"PRIu32, s_encoder.frame_size);

  // Set encoder parameters
  int tmp = SPEEX_QUALITY;
  speex_encoder_ctl(s_encoder.enc_state, SPEEX_SET_QUALITY, &tmp);
  
  tmp = SPEEX_COMPLEXITY;
  speex_encoder_ctl(s_encoder.enc_state, SPEEX_SET_COMPLEXITY, &tmp);

  tmp = SPEEX_SAMPLE_RATE;
  speex_encoder_ctl(s_encoder.enc_state, SPEEX_SET_SAMPLING_RATE, &tmp);
  VOICE_SPEEX_LOG("Set sample rate to: %d", tmp);

  tmp = SPEEX_BIT_RATE;
  speex_encoder_ctl(s_encoder.enc_state, SPEEX_SET_BITRATE, &tmp);
  VOICE_SPEEX_LOG("Set bit rate to: %d", tmp);

  // Get actual parameters
  int actual_sample_rate, actual_bit_rate;
  speex_encoder_ctl(s_encoder.enc_state, SPEEX_GET_SAMPLING_RATE, &actual_sample_rate);
  speex_encoder_ctl(s_encoder.enc_state, SPEEX_GET_BITRATE, &actual_bit_rate);
  
  s_encoder.sample_rate = (uint32_t)actual_sample_rate;
  s_encoder.bit_rate = (uint16_t)actual_bit_rate;
  
  s_encoder.bitstream_version = SPEEX_BITSTREAM_VERSION;

  // Allocate frame buffer (16-bit samples)
  s_encoder.frame_buffer_size = s_encoder.frame_size * sizeof(int16_t);
  s_encoder.frame_buffer = (uint8_t *)kernel_malloc_check(s_encoder.frame_buffer_size);

  // Allocate encoded buffer
  s_encoder.encoded_buffer_size = SPEEX_ENCODED_BUFFER_SIZE;
  s_encoder.encoded_buffer = kernel_malloc_check(s_encoder.encoded_buffer_size);

  s_encoder.initialized = true;

  VOICE_SPEEX_LOG("Speex encoder initialized: sample_rate=%"PRIu32", bit_rate=%"PRIu16", frame_size=%"PRIu32,
                  s_encoder.sample_rate, s_encoder.bit_rate, s_encoder.frame_size);
  
  // Verify sample rates match
  if (s_encoder.sample_rate != MIC_SAMPLE_RATE) {
    VOICE_SPEEX_LOG("WARNING: Speex sample rate (%"PRIu32") != Mic sample rate (%d)", 
                    s_encoder.sample_rate, MIC_SAMPLE_RATE);
  }

  return true;
}

void voice_speex_deinit(void) {
  if (!s_encoder.initialized) {
    return;
  }

  if (s_encoder.enc_state) {
    speex_encoder_destroy(s_encoder.enc_state);
    s_encoder.enc_state = NULL;
  }

  speex_bits_destroy(&s_encoder.bits);


  if (s_encoder.frame_buffer) {
    kernel_free(s_encoder.frame_buffer);
    s_encoder.frame_buffer = NULL;
  }

  if (s_encoder.encoded_buffer) {
    kernel_free(s_encoder.encoded_buffer);
    s_encoder.encoded_buffer = NULL;
  }

  memset(&s_encoder, 0, sizeof(s_encoder));
}

void voice_speex_get_transfer_info(AudioTransferInfoSpeex *info) {
  PBL_ASSERTN(s_encoder.initialized);
  PBL_ASSERTN(info);

  memset(info, 0, sizeof(AudioTransferInfoSpeex));
  strncpy(info->version, "1.2.1", sizeof(info->version) - 1);
  info->sample_rate = s_encoder.sample_rate;
  info->bit_rate = s_encoder.bit_rate;
  info->frame_size = (uint16_t)s_encoder.frame_size;  // Explicit cast to uint16_t
  info->bitstream_version = s_encoder.bitstream_version;
  
  VOICE_SPEEX_LOG("Transfer info: sample_rate=%"PRIu32", bit_rate=%"PRIu16", frame_size=%"PRIu16", bitstream_version=%"PRIu8, 
                  info->sample_rate, info->bit_rate, info->frame_size, info->bitstream_version);
  
  // Additional validation
  if (info->sample_rate != 16000) {
    VOICE_SPEEX_LOG("WARNING: Unexpected sample rate in transfer info: %"PRIu32, info->sample_rate);
  }
}

int voice_speex_get_frame_size(void) {
  return s_encoder.initialized ? (int)s_encoder.frame_size : 0;
}

int16_t *voice_speex_get_frame_buffer(void) {
  return s_encoder.initialized ? (int16_t *)s_encoder.frame_buffer : NULL;
}

size_t voice_speex_get_frame_buffer_size(void) {
  return s_encoder.initialized ? s_encoder.frame_buffer_size : 0;
}

int voice_speex_encode_frame(int16_t *samples, uint8_t *encoded_data, size_t max_encoded_size) {
  if (!s_encoder.initialized) {
    VOICE_SPEEX_LOG("ERROR: encode_frame called but Speex not initialized");
    return -1;
  }

  if (!samples || !encoded_data) {
    VOICE_SPEEX_LOG("ERROR: encode_frame called with invalid buffers");
    return -1;
  }

  // Apply gain boost to samples
  for (uint32_t i = 0; i < s_encoder.frame_size; i++) {
    int32_t boosted = (int32_t)samples[i] * SPEEX_AUDIO_GAIN;
    // Clamp to int16_t range to prevent overflow
    if (boosted > INT16_MAX) {
      boosted = INT16_MAX;
    } else if (boosted < INT16_MIN) {
      boosted = INT16_MIN;
    }
    samples[i] = (int16_t)boosted;
  }

  // Reset bits structure
  speex_bits_reset(&s_encoder.bits);

  // Encode frame (using 32-bit samples)
  speex_encode_int(s_encoder.enc_state, (spx_int16_t *)samples, &s_encoder.bits);

  // Write encoded data to buffer
  int encoded_bytes = speex_bits_write(&s_encoder.bits, (char *)encoded_data, max_encoded_size);
  
  if (encoded_bytes < 0) {
    VOICE_SPEEX_LOG("ERROR: Failed to write Speex encoded data (returned %d)", encoded_bytes);
    return -1;
  }

  VOICE_SPEEX_LOG("Encoded frame: input_samples=%"PRIu32", output_bytes=%d, frame_size=%"PRIu32, 
                  s_encoder.frame_size, encoded_bytes, s_encoder.frame_size);

  return encoded_bytes;
}

bool voice_speex_is_initialized(void) {
  return s_encoder.initialized;
}
