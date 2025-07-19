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

#pragma once

#include <stdint.h>
#include <stdbool.h>

// Opus codec configuration constants for embedded systems
#define OPUS_VOICE_SAMPLE_RATE    16000  // 16 kHz for voice
#define OPUS_VOICE_CHANNELS       1      // Mono for voice
#define OPUS_VOICE_FRAME_SIZE     320    // 20ms frame at 16kHz (320 samples)
#define OPUS_VOICE_FRAME_SIZE_MS  20     // Frame duration in milliseconds
#define OPUS_VOICE_BITRATE        16000  // 16 kbps for voice
#define OPUS_VOICE_MAX_PACKET     256    // Maximum encoded packet size

// Forward declarations for Opus types (from opus.h)
typedef struct OpusEncoder OpusEncoder;
typedef struct OpusDecoder OpusDecoder;

// Voice codec state structure
typedef struct {
    OpusEncoder *encoder;
    OpusDecoder *decoder;
    uint32_t sample_rate;
    uint16_t frame_size;
    uint16_t bit_rate;
    uint8_t channels;
    bool initialized;
} VoiceOpusCodec;

// Codec management functions
VoiceOpusCodec* voice_opus_create(void);
void voice_opus_destroy(VoiceOpusCodec *codec);
bool voice_opus_init_encoder(VoiceOpusCodec *codec, uint32_t sample_rate, uint16_t bitrate);
bool voice_opus_init_decoder(VoiceOpusCodec *codec, uint32_t sample_rate);

// Encoding/decoding functions  
int voice_opus_encode(VoiceOpusCodec *codec, const int16_t *pcm, uint8_t *encoded, int max_bytes);
int voice_opus_decode(VoiceOpusCodec *codec, const uint8_t *encoded, int encoded_bytes, 
                      int16_t *pcm, int frame_size, int decode_fec);

// Utility functions
int voice_opus_get_encoder_size(int channels);
int voice_opus_get_decoder_size(int channels);
const char* voice_opus_get_version_string(void);
