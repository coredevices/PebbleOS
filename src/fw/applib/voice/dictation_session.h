/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

//! @file voice/dictation_session.h
//! Defines the interface to the dictation session API
//! @addtogroup Microphone
//! @{
//!   @addtogroup DictationSession Dictation Session
//! \brief A dictation session allows the retrieval of a voice transcription from the Pebble
//! smartwatch's speech recognition provider via the same user interface used by the Pebble OS for
//! notifications.
//!
//! Starting a session will spawn the UI and upon user confirmation (unless this is disabled), the
//! result of the session as well as the transcription text will be returned via callback. If user
//! confirmation is disabled the first transcription result will be passed back via the callback.
//!
//! A dictation session must be created before use (see \ref dictation_session_create) and can
//! be reused for however many dictations are required, using \ref dictation_session_start. A
//! session can be aborted mid-flow by calling \ref dictation_session_stop.
//!
//! If these calls are made on a platform that does not support voice dictation,
//! \ref dictation_session_create will return NULL and the other calls will do nothing.

typedef struct DictationSession DictationSession;

// convenient macros to distinguish between mic and no mic.
// TODO: PBL-21978 remove redundant comments as a workaround around for SDK generator
#if defined(PBL_MICROPHONE)

//! Convenience macro to switch between two expressions depending on mic support.
//! On platforms with a mic the first expression will be chosen, the second otherwise.
#define PBL_IF_MICROPHONE_ELSE(if_true, if_false) (if_true)

#else

//! Convenience macro to switch between two expressions depending on mic support.
//! On platforms with a mic the first expression will be chosen, the second otherwise.
#define PBL_IF_MICROPHONE_ELSE(if_true, if_false) (if_false)
#endif

typedef enum {
  //! Transcription successful, with a valid result
  DictationSessionStatusSuccess,

  //! User rejected transcription and exited UI
  DictationSessionStatusFailureTranscriptionRejected,

  //! User exited UI after transcription error
  DictationSessionStatusFailureTranscriptionRejectedWithError,

  //! Too many errors occurred during transcription and the UI exited
  DictationSessionStatusFailureSystemAborted,

  //! No speech was detected and UI exited
  DictationSessionStatusFailureNoSpeechDetected,

  //! No BT or internet connection
  DictationSessionStatusFailureConnectivityError,

  //! Voice transcription disabled for this user
  DictationSessionStatusFailureDisabled,

  //! Voice transcription failed due to internal error
  DictationSessionStatusFailureInternalError,

  //! Cloud recognizer failed to transcribe speech (only possible if error dialogs disabled)
  DictationSessionStatusFailureRecognizerError,
} DictationSessionStatus;

//! Dictation status callback. Indicates success or failure of the dictation session and, if
//! successful, passes the transcribed string to the user of the dictation session. The transcribed
//! string will be freed after this call returns, so the string should be copied if it needs to be
//! retained afterwards.
//! @param session        dictation session from which the status was received
//! @param status         dictation status
//! @param transcription  transcribed string
//! @param context        callback context specified when starting the session
typedef void (*DictationSessionStatusCallback)(DictationSession *session,
                                               DictationSessionStatus status, char *transcription,
                                               void *context);

//! Create a dictation session. The session object can be used more than once to get a
//! transcription. When a transcription is received a buffer will be allocated to store the text in
//! with a maximum size specified by \ref buffer_size. When a transcription and accepted by the user
//! or a failure of some sort occurs, the callback specified will be called with the status and the
//! transcription if one was accepted.
//! @param buffer_size       size of buffer to allocate for the transcription text; text will be
//!                          truncated if it is longer than the maximum size specified; a size of 0
//!                          will allow the session to allocate as much as it needs and text will
//!                          not be truncated
//! @param callback          dictation session status handler (must be valid)
//! @param callback_context  context pointer for status handler
//! @return handle to the dictation session or NULL if the phone app is not connected or does not
//! support voice dictation, if this is called on a platform that doesn't support voice dictation,
//! or if an internal error occurs.
DictationSession *dictation_session_create(uint32_t buffer_size,
                                           DictationSessionStatusCallback callback,
                                           void *callback_context);

//! Destroy the dictation session and free its memory. Will terminate a session in progress.
//! @param session  dictation session to be destroyed
void dictation_session_destroy(DictationSession *session);

//! Enable or disable user confirmation of transcribed text, which allows the user to accept or
//! reject (and restart) the transcription. Must be called before the session is started.
//! @param session      dictation session to modify
//! @param is_enabled   set to true to enable user confirmation of transcriptions (default), false
//! to disable
void dictation_session_enable_confirmation(DictationSession *session, bool is_enabled);

//! Enable or disable error dialogs when transcription fails. Must be called before the session
//! is started. Disabling error dialogs will also disable automatic retries if transcription fails.
//! @param session      dictation session to modify
//! @param is_enabled   set to true to enable error dialogs (default), false to disable
void dictation_session_enable_error_dialogs(DictationSession *session, bool is_enabled);

//! Start the dictation session. The dictation UI will be shown. When the user accepts a
//! transcription or exits the UI, or, when the confirmation dialog is disabled and a status is
//! received, the status callback will be called. Can only be called when no session is in progress.
//! The session can be restarted multiple times after the UI is exited or the session is stopped.
//! @param session  dictation session to start or restart
//! @return true if session was started, false if session is already in progress or is invalid.
DictationSessionStatus dictation_session_start(DictationSession *session);

//! Stop the current dictation session. The UI will be hidden and no status callbacks will be
//! received after the session is stopped.
//! @param session  dictation session to stop
//! @return true if session was stopped, false if session was not started or is invalid
DictationSessionStatus dictation_session_stop(DictationSession *session);

//!   @} // end addtogroup DictationSession
//! @} // end addtogroup Microphone
