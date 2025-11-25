/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

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

// Sent before Speex encoded data
typedef struct PACKED {
  char version[20];
  uint32_t sample_rate;
  uint16_t bit_rate;
  uint8_t bitstream_version;
  uint16_t frame_size;
} AudioTransferInfoSpeex;

//! Called by the voice service to set up a dictation or command recognition session
void voice_endpoint_setup_session(VoiceEndpointSessionType session_type,
    AudioEndpointSessionId session_id, AudioTransferInfoSpeex *info, Uuid *app_uuid);
