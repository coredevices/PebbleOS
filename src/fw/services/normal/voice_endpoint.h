/*
 * Copyright 2024 Google LLC
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

#include "services/normal/audio_endpoint.h"
#include "services/normal/voice/transcription.h"
#include "util/attributes.h"
#include "util/uuid.h"

#include <inttypes.h>
#include <stdlib.h>

typedef enum {
  VoiceEndpointSessionTypeDictation = 0x01,
  VoiceEndpointSessionTypeCommand = 0x02,   // Not used yet
  VoiceEndpointSessionTypeNLP = 0x03,

  VoiceEndpointSessionTypeCount,
} VoiceEndpointSessionType;

typedef enum {
  VoiceEndpointResultSuccess = 0x00,
  VoiceEndpointResultFailServiceUnavailable = 0x01,
  VoiceEndpointResultFailTimeout = 0x02,
  VoiceEndpointResultFailRecognizerError = 0x03,
  VoiceEndpointResultFailInvalidRecognizerResponse = 0x04,
  VoiceEndpointResultFailDisabled = 0x05,
  VoiceEndpointResultFailInvalidMessage = 0x06,
} VoiceEndpointResult;

// Sent before Opus encoded data (maintaining compatibility with mobile app)
typedef struct PACKED {
  char version[20];
  uint32_t sample_rate;
  uint16_t bit_rate;
  uint8_t bitstream_version;
  uint16_t frame_size;
} AudioTransferInfoOpus;

//! Called by the voice service to set up a dictation or command recognition session
void voice_endpoint_setup_session(VoiceEndpointSessionType session_type,
    AudioEndpointSessionId session_id, AudioTransferInfoOpus *info, Uuid *app_uuid);
