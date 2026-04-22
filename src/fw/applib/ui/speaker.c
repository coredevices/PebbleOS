/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/speaker.h"

#include "applib/event_service_client.h"
#include "kernel/events.h"
#include "process_state/app_state/app_state.h"
#include "syscall/syscall.h"
#include "system/logging.h"

// MIDI note number for a given frequency (approximate, nearest semitone)
// Uses: midi = 69 + 12 * log2(freq/440)
static uint8_t prv_freq_to_midi(uint16_t freq_hz) {
  if (freq_hz == 0) {
    return 0;
  }

  // Simple lookup for common frequencies, otherwise approximate
  // Using integer math: find closest MIDI note
  // Standard: A4 = 440Hz = MIDI 69
  // Each semitone is freq * 2^(1/12) ~= freq * 1.0595

  // Binary search approach: start from A4 and adjust
  int16_t note = 69;
  uint32_t target = (uint32_t)freq_hz * 256;  // 8.8 fixed point

  // Find the right octave and semitone using ratio comparison
  // Semitone ratios * 1024 relative to octave base
  static const uint16_t semitone_ratio_x1024[] = {
    1024, 1085, 1149, 1217, 1290, 1366, 1448, 1534, 1625, 1722, 1824, 1933
  };

  // Find octave
  uint32_t base_freq = 440 * 256;  // A4 in 8.8
  int octave_offset = 0;

  while (target >= base_freq * 2 && note < 127) {
    base_freq *= 2;
    octave_offset++;
  }
  while (target < base_freq && note > 0) {
    base_freq /= 2;
    octave_offset--;
  }

  // Find semitone within octave
  int best_semi = 0;
  uint32_t best_diff = UINT32_MAX;
  for (int s = 0; s < 12; s++) {
    uint32_t semi_freq = (base_freq * semitone_ratio_x1024[s]) / 1024;
    uint32_t diff = (target > semi_freq) ? (target - semi_freq) : (semi_freq - target);
    if (diff < best_diff) {
      best_diff = diff;
      best_semi = s;
    }
  }

  note = 69 + octave_offset * 12 + best_semi;
  if (note < 0) note = 0;
  if (note > 127) note = 127;

  return (uint8_t)note;
}

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

  SpeakerNote note = {
    .midi_note = prv_freq_to_midi(frequency_hz),
    .waveform = (uint8_t)waveform,
    .duration_ms = (uint16_t)duration_ms,
    .velocity = 0,  // use global volume
    .reserved = 0,
  };

  return sys_speaker_play_note_seq(&note, 1, 0 /* SpeakerPriorityApp */, volume);
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
