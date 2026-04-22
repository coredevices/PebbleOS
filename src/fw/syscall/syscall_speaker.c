/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"

#include "kernel/pebble_tasks.h"
#include "pbl/services/normal/speaker/speaker_pcm_format.h"
#include "pbl/services/normal/speaker/speaker_service.h"
#include "pbl/services/normal/speaker/note_sequence.h"
#include "pbl/services/normal/speaker/track.h"
#include "system/passert.h"

#define SPEAKER_MAX_NOTES 256
#define SPEAKER_MAX_STREAM_WRITE 8192

DEFINE_SYSCALL(bool, sys_speaker_play_note_seq, const SpeakerNote *notes,
               uint32_t num_notes, uint8_t priority, uint8_t volume) {
  if (PRIVILEGE_WAS_ELEVATED) {
    if (num_notes > SPEAKER_MAX_NOTES) {
      syscall_failed();
    }
    syscall_assert_userspace_buffer(notes, num_notes * sizeof(SpeakerNote));
  }

  if (priority > SpeakerPriorityCritical) {
    // Apps can only use App priority
    priority = SpeakerPriorityApp;
  }

  PebbleTask task = pebble_task_get_current();
  speaker_service_set_owner_task(task);

  return speaker_service_play_note_seq(notes, num_notes,
                                       (SpeakerPriority)priority, volume);
}

DEFINE_SYSCALL(bool, sys_speaker_play_tracks, const SpeakerTrack *tracks,
               uint32_t num_tracks, uint8_t priority, uint8_t volume) {
  if (PRIVILEGE_WAS_ELEVATED) {
    if (num_tracks == 0 || num_tracks > SPEAKER_MAX_TRACKS) {
      syscall_failed();
    }
    syscall_assert_userspace_buffer(tracks, num_tracks * sizeof(SpeakerTrack));

    uint32_t total_sample_bytes = 0;
    for (uint32_t i = 0; i < num_tracks; i++) {
      if (tracks[i].num_notes == 0 || tracks[i].num_notes > SPEAKER_MAX_NOTES) {
        syscall_failed();
      }
      syscall_assert_userspace_buffer(tracks[i].notes,
                                      tracks[i].num_notes * sizeof(SpeakerNote));
      if (tracks[i].sample) {
        syscall_assert_userspace_buffer(tracks[i].sample, sizeof(SpeakerSample));
        uint32_t nb = tracks[i].sample->num_bytes;
        if (nb == 0) {
          syscall_failed();
        }
        total_sample_bytes += nb;
        if (total_sample_bytes > SPEAKER_MAX_SAMPLE_BYTES_TOTAL) {
          syscall_failed();
        }
        syscall_assert_userspace_buffer(tracks[i].sample->data, nb);
      }
    }
  }

  if (priority > SpeakerPriorityCritical) {
    priority = SpeakerPriorityApp;
  }

  PebbleTask task = pebble_task_get_current();
  speaker_service_set_owner_task(task);

  return speaker_service_play_tracks(tracks, num_tracks,
                                     (SpeakerPriority)priority, volume);
}

DEFINE_SYSCALL(bool, sys_speaker_stream_open, uint8_t priority, uint8_t volume,
               uint8_t format) {
  if (priority > SpeakerPriorityCritical) {
    priority = SpeakerPriorityApp;
  }

  if (format >= SpeakerPcmFormatCount) {
    format = SpeakerPcmFormat_16kHz_16bit;
  }

  PebbleTask task = pebble_task_get_current();
  speaker_service_set_owner_task(task);

  return speaker_service_stream_open((SpeakerPriority)priority, volume,
                                     (SpeakerPcmFormat)format);
}

DEFINE_SYSCALL(uint32_t, sys_speaker_stream_write, const void *data,
               uint32_t num_bytes) {
  if (PRIVILEGE_WAS_ELEVATED) {
    if (num_bytes > 8192) {
      syscall_failed();
    }
    syscall_assert_userspace_buffer(data, num_bytes);
  }

  return speaker_service_stream_write(data, num_bytes);
}

DEFINE_SYSCALL(void, sys_speaker_stream_close, void) {
  speaker_service_stream_close();
}

DEFINE_SYSCALL(void, sys_speaker_stop, void) {
  speaker_service_stop();
}

DEFINE_SYSCALL(void, sys_speaker_set_volume, uint8_t volume) {
  speaker_service_set_volume(volume);
}

DEFINE_SYSCALL(uint8_t, sys_speaker_get_state, void) {
  return (uint8_t)speaker_service_get_state();
}

DEFINE_SYSCALL(void, sys_speaker_register_finish, void) {
  speaker_service_register_finish(pebble_task_get_current());
}
