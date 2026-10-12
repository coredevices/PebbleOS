/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <stdlib.h>

#include <pbl/kernel/compiler.h>
#include <pbl/services/audio_endpoint.h>
#include <pbl/services/voice/transcription.h>
#include <pbl/util/uuid.h>

/**
 * @defgroup services_voice_endpoint Voice endpoint
 * @ingroup services
 * @brief Pebble Protocol voice control endpoint (11000).
 *
 * Sets up dictation and NLP sessions with the phone, whose audio is streamed through the audio
 * endpoint, and passes the results back to the voice service.
 * @{
 */

/** @brief Kind of voice session. */
typedef enum {
  /** Speech to text. */
  VoiceEndpointSessionTypeDictation = 0x01,
  /** Command recognition, not used yet. */
  VoiceEndpointSessionTypeCommand = 0x02,
  /** Natural language processing, e.g. reminders. */
  VoiceEndpointSessionTypeNLP = 0x03,
  //! Live audio for an app; the phone decodes and hands it to the app's companion, no result
  VoiceEndpointSessionTypeAudioStream = 0x04,

  /** Number of session types. */
  VoiceEndpointSessionTypeCount,
} VoiceEndpointSessionType;

/** @brief Result of a voice session, reported by the phone. */
typedef enum {
  /** Success. */
  VoiceEndpointResultSuccess = 0x00,
  /** The recognition service is unavailable. */
  VoiceEndpointResultFailServiceUnavailable = 0x01,
  /** Timed out. */
  VoiceEndpointResultFailTimeout = 0x02,
  /** The recognizer failed. */
  VoiceEndpointResultFailRecognizerError = 0x03,
  /** The recognizer response was invalid. */
  VoiceEndpointResultFailInvalidRecognizerResponse = 0x04,
  /** Voice is disabled on the phone. */
  VoiceEndpointResultFailDisabled = 0x05,
  /** The message was malformed. */
  VoiceEndpointResultFailInvalidMessage = 0x06,
} VoiceEndpointResult;

/** @brief Speex stream parameters, sent before the encoded audio. */
typedef struct PBL_PACKED {
  /** Speex version string. */
  char version[20];
  /** Sample rate in Hz. */
  uint32_t sample_rate;
  /** Bit rate in bits per second. */
  uint16_t bit_rate;
  /** Speex bitstream version. */
  uint8_t bitstream_version;
  /** Samples per frame. */
  uint16_t frame_size;
} AudioTransferInfoSpeex;

/**
 * @brief Ask the phone to start a voice session.
 *
 * Also switches the connection to its lowest latency for the duration of a voice session.
 *
 * @param session_type Kind of session.
 * @param session_id Audio endpoint session carrying the audio.
 * @param info Speex stream parameters.
 * @param app_uuid UUID of the requesting app, or NULL for a system session.
 */
void voice_endpoint_setup_session(VoiceEndpointSessionType session_type,
                                  AudioEndpointSessionId session_id, AudioTransferInfoSpeex *info,
                                  Uuid *app_uuid);

/** @} */
