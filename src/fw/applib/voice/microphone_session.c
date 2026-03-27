/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "microphone_session.h"

#include "applib/applib_malloc.auto.h"
#include "applib/event_service_client.h"
#include "kernel/events.h"
#include "process_management/app_install_manager.h"
#include "services/normal/voice/voice.h"
#include "syscall/syscall.h"
#include "system/logging.h"
#include "system/passert.h"

#if CAPABILITY_HAS_MICROPHONE

struct MicrophoneSession {
  MicrophoneSessionStatusCallback callback;
  void *context;
  VoiceSessionId session_id;
  bool in_progress;
  bool destroy_pending;
  EventServiceInfo voice_event_sub;
};

// Translate VoiceStatus → MicrophoneSessionStatus for failure cases.
static MicrophoneSessionStatus prv_status_from_voice_status(VoiceStatus voice_status) {
  switch (voice_status) {
    case VoiceStatusErrorConnectivity:
      return MicrophoneSessionStatusFailureConnectivityError;
    case VoiceStatusErrorDisabled:
      return MicrophoneSessionStatusFailureDisabled;
    default:
      return MicrophoneSessionStatusFailureInternalError;
  }
}

static void prv_finish_session(MicrophoneSession *session) {
  session->in_progress = false;
  session->session_id = VOICE_SESSION_ID_INVALID;
  event_service_client_unsubscribe(&session->voice_event_sub);

  if (session->destroy_pending) {
    microphone_session_destroy(session);
  }
}

static void prv_voice_event_handler(PebbleEvent *e, void *context) {
  MicrophoneSession *session = context;
  PBL_ASSERTN(session);

  const PebbleVoiceServiceEvent *ve = &e->voice_service;

  if (ve->type == VoiceEventTypeSessionSetup) {
    if (ve->status == VoiceStatusSuccess) {
      // Phone is ready; audio is now streaming.
      session->callback(session, MicrophoneSessionStatusRecording, session->context);
    } else {
      MicrophoneSessionStatus status = prv_status_from_voice_status(ve->status);
      prv_finish_session(session);
      session->callback(session, status, session->context);
    }
    return;
  }

  if (ve->type == VoiceEventTypeSessionResult) {
    MicrophoneSessionStatus status = (ve->status == VoiceStatusSuccess)
        ? MicrophoneSessionStatusSuccess
        : prv_status_from_voice_status(ve->status);
    prv_finish_session(session);
    session->callback(session, status, session->context);
    return;
  }
  // VoiceEventTypeSilenceDetected / VoiceEventTypeSpeechDetected are ignored for raw audio.
}

#endif // CAPABILITY_HAS_MICROPHONE

MicrophoneSession *microphone_session_create(MicrophoneSessionStatusCallback callback,
                                             void *callback_context) {
#if CAPABILITY_HAS_MICROPHONE
  if (!callback) {
    return NULL;
  }

  // Only allow app tasks to open raw audio sessions, and only when the phone supports the API.
  bool from_app = (pebble_task_get_current() == PebbleTask_App) &&
                  !app_install_id_from_system(sys_process_manager_get_current_process_id());
  if (from_app && !sys_system_pp_has_capability(CommSessionVoiceApiSupport)) {
    PBL_LOG_INFO("Phone not connected or does not support raw audio streaming");
    return NULL;
  }

  MicrophoneSession *session = applib_type_malloc(MicrophoneSession);
  if (!session) {
    return NULL;
  }

  *session = (MicrophoneSession) {
    .callback = callback,
    .context = callback_context,
    .session_id = VOICE_SESSION_ID_INVALID,
    .voice_event_sub = (EventServiceInfo) {
      .type = PEBBLE_VOICE_SERVICE_EVENT,
      .handler = prv_voice_event_handler,
      .context = session,
    },
  };

  return session;
#else
  return NULL;
#endif
}

void microphone_session_destroy(MicrophoneSession *session) {
#if CAPABILITY_HAS_MICROPHONE
  if (!session) {
    return;
  }

  if (session->in_progress) {
    // Can't destroy mid-stream; cancel and defer actual free until event fires.
    session->destroy_pending = true;
    sys_microphone_unsubscribe(session->session_id);
    return;
  }

  event_service_client_unsubscribe(&session->voice_event_sub);
  applib_free(session);
#endif
}

MicrophoneSessionStatus microphone_session_start(MicrophoneSession *session) {
#if CAPABILITY_HAS_MICROPHONE
  if (!session || session->in_progress) {
    return MicrophoneSessionStatusFailureInternalError;
  }

  event_service_client_subscribe(&session->voice_event_sub);

  VoiceSessionId sid = sys_microphone_subscribe();
  if (sid == VOICE_SESSION_ID_INVALID) {
    event_service_client_unsubscribe(&session->voice_event_sub);
    return MicrophoneSessionStatusFailureInternalError;
  }

  session->session_id = sid;
  session->in_progress = true;
  return MicrophoneSessionStatusRecording;
#else
  return MicrophoneSessionStatusFailureInternalError;
#endif
}

MicrophoneSessionStatus microphone_session_stop(MicrophoneSession *session) {
#if CAPABILITY_HAS_MICROPHONE
  if (!session || !session->in_progress) {
    return MicrophoneSessionStatusFailureInternalError;
  }

  sys_microphone_unsubscribe(session->session_id);
  // prv_finish_session is called when VoiceEventTypeSessionResult arrives.
  return MicrophoneSessionStatusSuccess;
#else
  return MicrophoneSessionStatusFailureInternalError;
#endif
}
