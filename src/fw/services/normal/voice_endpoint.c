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

#include "voice_endpoint.h"

#include "kernel/pbl_malloc.h"
#include "services/common/comm_session/session.h"
#include "services/normal/audio_endpoint.h"
#include "services/normal/voice/voice.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/generic_attribute.h"
#include "util/uuid.h"

#include <sys/types.h>

#include "voice_endpoint_private.h"

#define VOICE_CONTROL_ENDPOINT (11000)

#if CAPABILITY_HAS_MICROPHONE
static bool prv_handle_result_common(VoiceEndpointResult result,
                                     bool app_initiated,
                                     AudioEndpointSessionId session_id,
                                     GenericAttributeList *attr_list,
                                     size_t attr_list_size,
                                     Uuid **app_uuid_out) {

  GenericAttribute *uuid_attr = generic_attribute_find_attribute(attr_list,
                                                                 VEAttributeIdAppUuid,
                                                                 attr_list_size);
  if (app_initiated && !uuid_attr) {
    PBL_LOG(LOG_LEVEL_WARNING, "No app UUID found for dictation response from app-initiated "
        "session");
    voice_handle_dictation_result(VoiceEndpointResultFailInvalidMessage, session_id, NULL,
                                  app_initiated, NULL);
    return false;
  }

  Uuid *app_uuid = uuid_attr ? (Uuid *)uuid_attr->data : NULL;

  if (result != VoiceEndpointResultSuccess) {
    voice_handle_dictation_result(result, session_id, NULL, app_initiated, app_uuid);
    return false;
  }

  if (attr_list->num_attributes == 0) {
    PBL_LOG(LOG_LEVEL_WARNING, "No attributes in message");
    voice_handle_dictation_result(VoiceEndpointResultFailInvalidMessage, session_id, NULL,
                                  app_initiated, app_uuid);
    return false;
  }

  *app_uuid_out = app_uuid;
  return true;
}

static void prv_handle_dictation_result(VoiceSessionResultMsg *msg, size_t size) {
  const size_t attr_list_size = size - sizeof(VoiceSessionResultMsg) + sizeof(GenericAttributeList);
  const bool app_initiated = (msg->flags.app_initiated == 1);
  Uuid *app_uuid = NULL;

  if (!prv_handle_result_common(msg->result, app_initiated, msg->session_id,
                                &msg->attr_list, attr_list_size, &app_uuid)) {
    return;
  }

  GenericAttribute *transcription_attr = generic_attribute_find_attribute(&msg->attr_list,
      VEAttributeIdTranscription, attr_list_size);

  if (!transcription_attr || transcription_attr->length == 0) {
    PBL_LOG(LOG_LEVEL_WARNING, "No transcription attribute found");
    voice_handle_dictation_result(VoiceEndpointResultFailInvalidMessage, msg->session_id, NULL,
                                  app_initiated, app_uuid);
    return;
  }

  Transcription *transcription = (Transcription *)transcription_attr->data;
  bool valid = transcription_validate(transcription, transcription_attr->length);

  if (!valid) {
    PBL_LOG(LOG_LEVEL_WARNING, "Unrecognized transcription format received");
    voice_handle_dictation_result(VoiceEndpointResultFailInvalidRecognizerResponse,
                                  msg->session_id, NULL, app_initiated, app_uuid);
  }
  voice_handle_dictation_result(msg->result, msg->session_id, transcription,
                                app_initiated, app_uuid);
}

static void prv_handle_nlp_result(VoiceSessionResultMsg *msg, size_t size) {
  const size_t attr_list_size = size - sizeof(VoiceSessionResultMsg) + sizeof(GenericAttributeList);
  const bool app_initiated = (msg->flags.app_initiated == 1);
  Uuid *app_uuid = NULL;

  if (!prv_handle_result_common(msg->result, app_initiated, msg->session_id,
                                &msg->attr_list, attr_list_size, &app_uuid)) {
    return;
  }
  if (app_uuid) {
    PBL_LOG(LOG_LEVEL_WARNING, "Got an app UUID in a NLP result msg. Ignoring and continuing");
  }


  // The timestamp attribute is optional
  time_t timestamp = 0;
  GenericAttribute *timestamp_attr = generic_attribute_find_attribute(&msg->attr_list,
      VEAttributeIdTimestamp, attr_list_size);
  if (timestamp_attr && timestamp_attr->length == sizeof(uint32_t)) {
    uint32_t *timestamp_ptr = (uint32_t*)timestamp_attr->data;
    timestamp = *timestamp_ptr;
  }

  GenericAttribute *reminder_attr = generic_attribute_find_attribute(&msg->attr_list,
      VEAttributeIdReminder, attr_list_size);

  if (!reminder_attr || reminder_attr->length == 0) {
    PBL_LOG(LOG_LEVEL_WARNING, "No reminder attribute found");
    voice_handle_nlp_result(VoiceEndpointResultFailInvalidMessage, msg->session_id, NULL, 0);
    return;
  }
  char *reminder_str = kernel_zalloc_check(reminder_attr->length + 1);
  memcpy(reminder_str, reminder_attr->data, reminder_attr->length);
  reminder_str[reminder_attr->length] = '\0';

  voice_handle_nlp_result(msg->result, msg->session_id, reminder_str, timestamp);
  kernel_free(reminder_str);
}
#endif

#if CAPABILITY_HAS_MICROPHONE
void voice_endpoint_protocol_msg_callback(CommSession *session, const uint8_t* data, size_t size) {
  MsgId msg_id = data[0];
  switch (msg_id) {
    case MsgIdSessionSetup: {
      if (size >= sizeof(SessionSetupResultMsg)) {
        SessionSetupResultMsg *msg = (SessionSetupResultMsg *) data;
        
        // Validate result enum value to prevent crashes from invalid values
        VoiceEndpointResult result = msg->result;
        if (result > VoiceEndpointResultFailInvalidMessage) {
          PBL_LOG(LOG_LEVEL_ERROR, "Invalid VoiceEndpointResult value: %d, treating as invalid message", 
                  (int)result);
          result = VoiceEndpointResultFailInvalidMessage;
        }
        
        bool app_initiated = (msg->flags.app_initiated == 1);
        voice_handle_session_setup_result(result, msg->session_type, app_initiated);
      } else {
        PBL_LOG(LOG_LEVEL_WARNING, "Invalid size for session setup result message");
      }
      break;
    }
    case MsgIdDictationResult: {
      if (size >= sizeof(VoiceSessionResultMsg)) {
        VoiceSessionResultMsg *msg = (VoiceSessionResultMsg *) data;
        prv_handle_dictation_result(msg, size);
      } else {
        PBL_LOG(LOG_LEVEL_WARNING, "Invalid size for dictation result message %zu", size);
      }
      break;
    }
    case MsgIdNLPResult: {
      if (size >= sizeof(VoiceSessionResultMsg)) {
        VoiceSessionResultMsg *msg = (VoiceSessionResultMsg *) data;
        prv_handle_nlp_result(msg, size);
      } else {
        PBL_LOG(LOG_LEVEL_WARNING, "Invalid size for dictation result message %zu", size);
      }
      break;
    }
    default:
      // Ignore invalid message ID
      PBL_LOG(LOG_LEVEL_WARNING, "Invalid message ID");
      break;
  }

}
#else
void voice_endpoint_protocol_msg_callback(CommSession *session, const uint8_t* data, size_t size) {
}
#endif

void voice_endpoint_setup_session(VoiceEndpointSessionType session_type,
    AudioEndpointSessionId session_id, AudioTransferInfoSpeex *info, Uuid *app_uuid) {

  CommSession *comm_session = comm_session_get_system_session();
  comm_session_set_responsiveness(comm_session, BtConsumerPpVoiceEndpoint, ResponseTimeMin,
                                  MIN_LATENCY_MODE_TIMEOUT_VOICE_SECS);

  // We're only sending one attribute now: the speex audio transfer info packet
  size_t size = sizeof(SessionSetupMsg) + sizeof(GenericAttribute) +
                sizeof(AudioTransferInfoSpeex) +
                (app_uuid ? (sizeof(Uuid) + sizeof(GenericAttribute)) : 0);
  SessionSetupMsg *msg = kernel_malloc_check(size);
  *msg = (SessionSetupMsg) {
    .msg_id = MsgIdSessionSetup,
    .session_type = session_type,
    .session_id = session_id,
    .attr_list.num_attributes = 1,
  };

  GenericAttribute *attr = msg->attr_list.attributes;
  if (app_uuid) {
    // set this after struct initialization because the rest of the fields in the bitfield are left
    // uninitialized if just one is set.
    msg->flags.app_initiated = 1;

    // we're also sending the app UUID
    msg->attr_list.num_attributes += 1;

    // add app UUID attribute
    attr = generic_attribute_add_attribute(attr, VEAttributeIdAppUuid, app_uuid, sizeof(Uuid));
  }

  attr = generic_attribute_add_attribute(attr, VEAttributeIdAudioTransferInfoSpeex, info,
      sizeof(AudioTransferInfoSpeex));

  size_t actual_size = (uint8_t *)attr - (uint8_t *)msg;
  PBL_ASSERTN(actual_size == size);

  comm_session_send_data(comm_session, VOICE_CONTROL_ENDPOINT, (uint8_t *)msg,
                         size, COMM_SESSION_DEFAULT_TIMEOUT);
  kernel_free(msg);
}
