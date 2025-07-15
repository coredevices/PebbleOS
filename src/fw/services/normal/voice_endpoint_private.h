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
#include "services/normal/voice_endpoint.h"
#include "util/attributes.h"
#include "util/generic_attribute.h"

// Shared message definitions with unit test

typedef enum {
  MsgIdSessionSetup = 0x01,
  MsgIdDictationResult = 0x02,
  MsgIdNLPResult = 0x03,
} MsgId;

// Attribute ID definitions
typedef enum {
  VEAttributeIdInvalid = 0x00,
  VEAttributeIdAudioTransferInfoOpus = 0x01,
  VEAttributeIdTranscription = 0x02,
  VEAttributeIdAppUuid = 0x03,
  VEAttributeIdReminder = 0x04,
  VEAttributeIdTimestamp = 0x05,
} VEAttributeId;

// Sent and received by watch. Result is only sent by phone.

typedef union PACKED {
  struct {
    uint32_t app_initiated:1;
  };
  uint32_t all;
} VEFlags;

typedef struct PACKED {
  MsgId msg_id:8;
  VEFlags flags;
  VoiceEndpointSessionType session_type:8;
  AudioEndpointSessionId session_id;
  GenericAttributeList attr_list;
} SessionSetupMsg;

typedef struct PACKED {
  MsgId msg_id:8;
  VEFlags flags;
  VoiceEndpointSessionType session_type:8;
  VoiceEndpointResult result:8;
} SessionSetupResultMsg;

typedef struct PACKED {
  MsgId msg_id:8;
  VEFlags flags;
  AudioEndpointSessionId session_id;
  VoiceEndpointResult result:8;
  GenericAttributeList attr_list;
} VoiceSessionResultMsg;
