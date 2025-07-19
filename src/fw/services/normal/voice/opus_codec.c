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

#include "opus_codec.h"
#include "kernel/pbl_malloc.h"
#include "system/logging.h"
#include "system/passert.h"

#include <opus.h>
#include <string.h>

VoiceOpusCodec* voice_opus_create(void) {
    VoiceOpusCodec *codec = kernel_malloc_check(sizeof(VoiceOpusCodec));
    memset(codec, 0, sizeof(VoiceOpusCodec));
    return codec;
}

void voice_opus_destroy(VoiceOpusCodec *codec) {
    if (!codec) {
        return;
    }
    
    if (codec->encoder) {
        opus_encoder_destroy(codec->encoder);
        codec->encoder = NULL;
    }
    
    if (codec->decoder) {
        opus_decoder_destroy(codec->decoder);
        codec->decoder = NULL;
    }
    
    codec->initialized = false;
    kernel_free(codec);
}

bool voice_opus_init_encoder(VoiceOpusCodec *codec, uint32_t sample_rate, uint16_t bitrate) {
    if (!codec) {
        return false;
    }
    
    int error;
    
    // Create encoder for voice application
    codec->encoder = opus_encoder_create(sample_rate, OPUS_VOICE_CHANNELS, OPUS_APPLICATION_VOIP, &error);
    if (error != OPUS_OK || !codec->encoder) {
        PBL_LOG(LOG_LEVEL_ERROR, "Failed to create Opus encoder: %s", opus_strerror(error));
        return false;
    }
    
    // Configure encoder for embedded voice use
    opus_encoder_ctl(codec->encoder, OPUS_SET_BITRATE(bitrate));
    opus_encoder_ctl(codec->encoder, OPUS_SET_VBR(1));  // Variable bitrate
    opus_encoder_ctl(codec->encoder, OPUS_SET_VBR_CONSTRAINT(1));  // Constrained VBR
    opus_encoder_ctl(codec->encoder, OPUS_SET_COMPLEXITY(1));  // Low complexity for embedded
    opus_encoder_ctl(codec->encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));  // Voice signal
    opus_encoder_ctl(codec->encoder, OPUS_SET_DTX(1));  // Discontinuous transmission
    opus_encoder_ctl(codec->encoder, OPUS_SET_INBAND_FEC(1));  // Forward error correction
    opus_encoder_ctl(codec->encoder, OPUS_SET_PACKET_LOSS_PERC(5));  // Expected 5% packet loss
    
    codec->sample_rate = sample_rate;
    codec->bit_rate = bitrate;
    codec->channels = OPUS_VOICE_CHANNELS;
    codec->frame_size = sample_rate * OPUS_VOICE_FRAME_SIZE_MS / 1000;  // Frame size in samples
     PBL_LOG(LOG_LEVEL_INFO, "Opus encoder initialized: %luHz, %lu kbps, %u samples/frame",
            (unsigned long)sample_rate, (unsigned long)(bitrate/1000), codec->frame_size);
    
    return true;
}

bool voice_opus_init_decoder(VoiceOpusCodec *codec, uint32_t sample_rate) {
    if (!codec) {
        return false;
    }
    
    int error;
    
    // Create decoder
    codec->decoder = opus_decoder_create(sample_rate, OPUS_VOICE_CHANNELS, &error);
    if (error != OPUS_OK || !codec->decoder) {
        PBL_LOG(LOG_LEVEL_ERROR, "Failed to create Opus decoder: %s", opus_strerror(error));
        return false;
    }
    
    if (!codec->sample_rate) {
        codec->sample_rate = sample_rate;
        codec->channels = OPUS_VOICE_CHANNELS;
        codec->frame_size = sample_rate * OPUS_VOICE_FRAME_SIZE_MS / 1000;
    }
     PBL_LOG(LOG_LEVEL_INFO, "Opus decoder initialized: %luHz, %u samples/frame",
            (unsigned long)sample_rate, codec->frame_size);
    
    return true;
}

int voice_opus_encode(VoiceOpusCodec *codec, const int16_t *pcm, uint8_t *encoded, int max_bytes) {
    if (!codec || !codec->encoder || !pcm || !encoded) {
        return -1;
    }
    
    int encoded_bytes = opus_encode(codec->encoder, pcm, codec->frame_size, encoded, max_bytes);
    
    if (encoded_bytes < 0) {
        PBL_LOG(LOG_LEVEL_ERROR, "Opus encoding failed: %s", opus_strerror(encoded_bytes));
        return -1;
    }
    
    PBL_LOG(LOG_LEVEL_DEBUG, "Encoded %u samples to %d bytes", codec->frame_size, encoded_bytes);
    return encoded_bytes;
}

int voice_opus_decode(VoiceOpusCodec *codec, const uint8_t *encoded, int encoded_bytes,
                      int16_t *pcm, int frame_size, int decode_fec) {
    if (!codec || !codec->decoder || !pcm) {
        return -1;
    }
    
    int decoded_samples = opus_decode(codec->decoder, encoded, encoded_bytes, pcm, frame_size, decode_fec);
    
    if (decoded_samples < 0) {
        PBL_LOG(LOG_LEVEL_ERROR, "Opus decoding failed: %s", opus_strerror(decoded_samples));
        return -1;
    }
    
    PBL_LOG(LOG_LEVEL_DEBUG, "Decoded %d bytes to %d samples", encoded_bytes, decoded_samples);
    return decoded_samples;
}

int voice_opus_get_encoder_size(int channels) {
    return opus_encoder_get_size(channels);
}

int voice_opus_get_decoder_size(int channels) {
    return opus_decoder_get_size(channels);
}

const char* voice_opus_get_version_string(void) {
    return opus_get_version_string();
}
