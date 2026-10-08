/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/services/audio_endpoint.h>
#include <pbl/services/voice_endpoint.h>
#include <pbl/util/generic_attr.h>

/**
 * @addtogroup services_voice_endpoint
 * @{
 */

/** @brief Voice endpoint message identifier. */
typedef enum {
  /** Session setup request (watch) or result (phone). */
  MsgIdSessionSetup = 0x01,
  /** Dictation result, from the phone. */
  MsgIdDictationResult = 0x02,
  /** NLP result, from the phone. */
  MsgIdNLPResult = 0x03,
} MsgId;

/** @brief Attribute identifier in voice endpoint messages. */
typedef enum {
  /** Invalid. */
  VEAttributeIdInvalid = 0x00,
  /** AudioTransferInfoSpeex. */
  VEAttributeIdAudioTransferInfoSpeex = 0x01,
  /** Transcription. */
  VEAttributeIdTranscription = 0x02,
  /** UUID of the app that started the session. */
  VEAttributeIdAppUuid = 0x03,
  /** Reminder text of an NLP result. */
  VEAttributeIdReminder = 0x04,
  /** 32-bit timestamp of an NLP result. */
  VEAttributeIdTimestamp = 0x05,
} VEAttributeId;

/** @brief Message flags. */
typedef union PBL_PACKED {
  struct {
    /** The session was started by an app. */
    uint32_t app_initiated : 1;
  };
  /** All flags. */
  uint32_t all;
} VEFlags;

/** @brief Session setup request, from the watch. */
typedef struct PBL_PACKED {
  /** #MsgIdSessionSetup. */
  MsgId msg_id : 8;
  /** Flags. */
  VEFlags flags;
  /** Kind of session. */
  VoiceEndpointSessionType session_type : 8;
  /** Audio endpoint session. */
  AudioEndpointSessionId session_id;
  /** Attributes: speex info and, for apps, the app UUID. */
  struct pbl_generic_attr_list attr_list;
} SessionSetupMsg;

/** @brief Session setup result, from the phone. */
typedef struct PBL_PACKED {
  /** #MsgIdSessionSetup. */
  MsgId msg_id : 8;
  /** Flags. */
  VEFlags flags;
  /** Kind of session. */
  VoiceEndpointSessionType session_type : 8;
  /** Setup result. */
  VoiceEndpointResult result : 8;
} SessionSetupResultMsg;

/** @brief Dictation or NLP result, from the phone. */
typedef struct PBL_PACKED {
  /** #MsgIdDictationResult or #MsgIdNLPResult. */
  MsgId msg_id : 8;
  /** Flags. */
  VEFlags flags;
  /** Audio endpoint session. */
  AudioEndpointSessionId session_id;
  /** Session result. */
  VoiceEndpointResult result : 8;
  /** Result attributes. */
  struct pbl_generic_attr_list attr_list;
} VoiceSessionResultMsg;

/** @} */
