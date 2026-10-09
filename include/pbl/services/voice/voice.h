/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/audio_endpoint.h>
#include <pbl/services/voice/transcription.h>
#include <pbl/services/voice_endpoint.h>

#include <applib/graphics/utf8.h>
#include <kernel/pebble_tasks.h>
#include <sys/types.h>

/**
 * @defgroup services_voice Voice
 * @ingroup services
 * @brief Dictation sessions: microphone capture, Speex encoding and transcription by the phone.
 *
 * One session runs at a time. Starting a session sets it up with the phone over the voice and
 * audio endpoints; once both are ready the microphone is started and Speex frames are streamed.
 * Progress is reported with @c PEBBLE_VOICE_SERVICE_EVENT events: a @c VoiceEventTypeSessionSetup
 * event when the session is set up or fails to, then a @c VoiceEventTypeSessionResult event with
 * the transcribed text or an error.
 *
 * @code{.c}
 * static VoiceSessionId s_session;
 *
 * void start(void) {
 *   s_session = voice_start_dictation(VoiceEndpointSessionTypeDictation);
 * }
 *
 * // On PEBBLE_VOICE_SERVICE_EVENT:
 * void handle_voice_event(const PebbleVoiceServiceEvent *e) {
 *   if (e->type == VoiceEventTypeSessionSetup && e->status == VoiceStatusSuccess) {
 *     // Recording; call voice_stop_dictation(s_session) when the user is done.
 *   } else if (e->type == VoiceEventTypeSessionResult && e->status == VoiceStatusSuccess) {
 *     use_text(e->data->sentence);
 *   }
 * }
 * @endcode
 * @{
 */

/** @brief Outcome reported in voice service events. */
typedef enum {
  /** Session set up, or transcription received. */
  VoiceStatusSuccess,
  /** The phone did not answer in time. */
  VoiceStatusTimeout,
  /** Unexpected or malformed response, or local failure. */
  VoiceStatusErrorGeneric,
  /** The phone reports the recognition service is unavailable. */
  VoiceStatusErrorConnectivity,
  /** Voice is disabled on the phone. */
  VoiceStatusErrorDisabled,
  /** The recognizer returned an invalid response. */
  VoiceStatusRecognizerResponseError,
} VoiceStatus;

/** @brief Dictation session identifier, the audio endpoint transfer session id. */
typedef AudioEndpointSessionId VoiceSessionId;

/** @brief Invalid session id, returned when a session cannot be started. */
#define VOICE_SESSION_ID_INVALID AUDIO_ENDPOINT_SESSION_INVALID_ID

/**
 * @brief Start a dictation session.
 *
 * Lazily initializes the Speex encoder, sets the session up with the phone and starts streaming
 * audio once it is ready. Then a @c VoiceEventTypeSessionSetup event is sent with:
 * - @ref VoiceStatusSuccess when recording has started;
 * - an error status when the phone refuses the session or recording cannot start;
 * - @ref VoiceStatusTimeout when the phone does not answer within 8 s.
 *
 * If the phone stops the recording itself, a @c VoiceEventTypeSessionResult event follows as
 * described in voice_stop_dictation(). When called from a third-party app, the app UUID is sent
 * to the phone and the session is cancelled if the app exits.
 *
 * @param session_type Type of session (dictation or NLP).
 * @return Session id, or @ref VOICE_SESSION_ID_INVALID if a session is already in progress or
 * the encoder could not be initialized.
 */
VoiceSessionId voice_start_dictation(VoiceEndpointSessionType session_type);

/**
 * @brief Stop recording and wait for the transcription.
 *
 * Stops the microphone and the audio transfer. A @c VoiceEventTypeSessionResult event is then
 * sent with:
 * - @ref VoiceStatusSuccess and the text when the phone returns the transcription;
 * - an error status when the phone reports an error;
 * - @ref VoiceStatusTimeout when no result arrives within 15 s.
 *
 * Called before recording has started, the session is cancelled instead. Ignored if
 * @p session_id is not the current session.
 *
 * @param session_id Session returned by voice_start_dictation().
 */
void voice_stop_dictation(VoiceSessionId session_id);

/**
 * @brief Cancel a dictation session at any stage.
 *
 * No further event is sent for the session. Ignored if @p session_id is not the current session.
 *
 * @param session_id Session returned by voice_start_dictation().
 */
void voice_cancel_dictation(VoiceSessionId session_id);

/** @brief Initialize the voice service. */
void voice_init(void);

/**
 * @brief Handle a session setup result received by the voice endpoint.
 *
 * @param result Result code for the session setup.
 * @param session_type Type of session.
 * @param app_initiated True if the session was initiated by an app.
 */
void voice_handle_session_setup_result(VoiceEndpointResult result,
                                       VoiceEndpointSessionType session_type, bool app_initiated);

/**
 * @brief Handle a dictation result received by the voice endpoint.
 *
 * Ends the session. On success, the words of the first sentence are joined into a string carried
 * by the @c VoiceEventTypeSessionResult event; @p transcription is not referenced afterwards.
 *
 * @param result Result code for the dictation session.
 * @param session_id Audio transfer session the transcription was derived from.
 * @param transcription Transcription, already checked with transcription_validate().
 * @param app_initiated True if the session was initiated by an app.
 * @param app_uuid UUID of the initiating app, compared with the expected one. Not retained.
 */
void voice_handle_dictation_result(VoiceEndpointResult result, AudioEndpointSessionId session_id,
                                   Transcription *transcription, bool app_initiated,
                                   Uuid *app_uuid);

/**
 * @brief Handle an NLP (reminder) result received by the voice endpoint.
 *
 * Ends the session. On success, the @c VoiceEventTypeSessionResult event carries a copy of
 * @p reminder and @p timestamp.
 *
 * @param result Result code for the session.
 * @param session_id Audio transfer session the result was derived from.
 * @param reminder Reminder text, zero terminated.
 * @param timestamp Reminder time.
 */
void voice_handle_nlp_result(VoiceEndpointResult result, AudioEndpointSessionId session_id,
                             char *reminder, time_t timestamp);

/**
 * @brief Syscall wrapper of voice_start_dictation().
 *
 * @param session_type Type of session; out of range values are rejected.
 * @return Session id, or @ref VOICE_SESSION_ID_INVALID.
 */
VoiceSessionId sys_voice_start_dictation(VoiceEndpointSessionType session_type);

/**
 * @brief Syscall wrapper of voice_stop_dictation().
 *
 * @param session_id Session returned by voice_start_dictation().
 */
void sys_voice_stop_dictation(VoiceSessionId session_id);

/**
 * @brief Syscall wrapper of voice_cancel_dictation().
 *
 * @param session_id Session returned by voice_start_dictation().
 */
void sys_voice_cancel_dictation(VoiceSessionId session_id);

/**
 * @brief Cancel the session started by an app that is exiting.
 *
 * @param task Exiting task; only @c PebbleTask_App sessions are cancelled.
 */
void voice_kill_app_session(PebbleTask task);

/** @} */
