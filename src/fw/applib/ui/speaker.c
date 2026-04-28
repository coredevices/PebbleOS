/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/speaker.h"

#include "applib/event_service_client.h"
#include "kernel/events.h"
#include "process_state/app_state/app_state.h"
#include "syscall/syscall.h"
#include "system/logging.h"

bool speaker_play_notes(const SpeakerNote *notes, uint32_t num_notes, uint8_t volume) {
  if (!notes || num_notes == 0) {
    PBL_LOG_ERR("tried to play null or empty note sequence");
    return false;
  }

  return sys_speaker_play_note_seq(notes, num_notes, 0 /* SpeakerPriorityApp */, volume);
}

bool speaker_play_tone(uint16_t frequency_hz, uint32_t duration_ms,
                       uint8_t volume, SpeakerWaveform waveform) {
  if (duration_ms > 10000) {
    duration_ms = 10000;
  }

  return sys_speaker_play_tone(frequency_hz, (uint16_t)duration_ms,
                               (uint8_t)waveform, 0 /* use global volume */,
                               0 /* SpeakerPriorityApp */, volume);
}

bool speaker_stream_open(SpeakerPcmFormat format, uint8_t volume) {
  return sys_speaker_stream_open(0 /* SpeakerPriorityApp */, volume, (uint8_t)format);
}

uint32_t speaker_stream_write(const void *data, uint32_t num_bytes) {
  if (!data || num_bytes == 0) {
    return 0;
  }

  return sys_speaker_stream_write(data, num_bytes);
}

void speaker_stream_close(void) {
  sys_speaker_stream_close();
}

void speaker_stop(void) {
  sys_speaker_stop();
}

void speaker_set_volume(uint8_t volume) {
  sys_speaker_set_volume(volume);
}

SpeakerStatus speaker_get_status(void) {
  return (SpeakerStatus)sys_speaker_get_state();
}

bool speaker_play_tracks(const SpeakerTrack *tracks, uint32_t num_tracks, uint8_t volume) {
  if (!tracks || num_tracks == 0) {
    PBL_LOG_ERR("tried to play null or empty track list");
    return false;
  }

  return sys_speaker_play_tracks(tracks, num_tracks, 0 /* SpeakerPriorityApp */, volume);
}

static void prv_finish_event_handler(PebbleEvent *e, void *context) {
  SpeakerFinishedCallback cb = app_state_get_speaker_finish_handler();
  if (cb == NULL) {
    return;
  }
  cb((SpeakerFinishReason)e->speaker.finish_reason,
     app_state_get_speaker_finish_ctx());
}

void speaker_set_finish_callback(SpeakerFinishedCallback cb, void *ctx) {
  EventServiceInfo *info = app_state_get_speaker_finish_event_info();
  SpeakerFinishedCallback prev = app_state_get_speaker_finish_handler();

  if (cb) {
    app_state_set_speaker_finish_handler(cb);
    app_state_set_speaker_finish_ctx(ctx);
    if (prev == NULL) {
      // First registration — subscribe and tell the service to post events.
      info->type = PEBBLE_SPEAKER_EVENT;
      info->handler = prv_finish_event_handler;
      event_service_client_subscribe(info);
      sys_speaker_register_finish();
    }
  } else {
    if (prev != NULL) {
      event_service_client_unsubscribe(info);
    }
    app_state_set_speaker_finish_handler(NULL);
    app_state_set_speaker_finish_ctx(NULL);
  }
}
