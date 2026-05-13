/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"

#include "kernel/pebble_tasks.h"
#include "pbl/services/speaker/speaker_pcm_format.h"
#include "pbl/services/speaker/speaker_service.h"
#include "pbl/services/speaker/note_sequence.h"
#include "pbl/services/speaker/track.h"
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

DEFINE_SYSCALL(bool, sys_speaker_play_tone, uint16_t freq_hz,
               uint16_t duration_ms, uint8_t waveform, uint8_t velocity,
               uint8_t priority, uint8_t volume) {
  if (priority > SpeakerPriorityCritical) {
    priority = SpeakerPriorityApp;
  }

  if (waveform >= SpeakerWaveformCount) {
    syscall_failed();
  }

  if (velocity > 127) {
    syscall_failed();
  }

  PebbleTask task = pebble_task_get_current();
  speaker_service_set_owner_task(task);

  return speaker_service_play_tone(freq_hz, duration_ms, waveform, velocity,
                                   (SpeakerPriority)priority, volume);
}

DEFINE_SYSCALL(bool, sys_speaker_play_tracks, const SpeakerTrack *tracks,
               uint32_t num_tracks, uint8_t priority, uint8_t volume) {
  // We need to defend against TOCTOU: speaker_service_play_tracks() re-reads
  // each track's num_notes and sample->num_bytes from user memory (e.g. as the
  // size argument to kernel_malloc/memcpy that copy the user buffers into
  // kernel buffers). A racing app could resize them after our validation and
  // overflow either the kernel allocation or the validated source range.
  //
  // Take a kernel-stack copy of the track table (and embedded sample structs)
  // up front and pass the kernel copy downstream. The notes/data pointers
  // themselves still aim into user memory, but the sizes are now stable.
  SpeakerTrack kernel_tracks[SPEAKER_MAX_TRACKS];
  SpeakerSample kernel_samples[SPEAKER_MAX_TRACKS];
  const SpeakerTrack *tracks_to_play = tracks;

  if (PRIVILEGE_WAS_ELEVATED) {
    if (num_tracks == 0 || num_tracks > SPEAKER_MAX_TRACKS) {
      syscall_failed();
    }
    syscall_assert_userspace_buffer(tracks, num_tracks * sizeof(SpeakerTrack));

    uint32_t total_sample_bytes = 0;
    for (uint32_t i = 0; i < num_tracks; i++) {
      kernel_tracks[i] = tracks[i];
      if (kernel_tracks[i].num_notes == 0 || kernel_tracks[i].num_notes > SPEAKER_MAX_NOTES) {
        syscall_failed();
      }
      syscall_assert_userspace_buffer(kernel_tracks[i].notes,
                                      kernel_tracks[i].num_notes * sizeof(SpeakerNote));
      if (kernel_tracks[i].sample) {
        syscall_assert_userspace_buffer(kernel_tracks[i].sample, sizeof(SpeakerSample));
        kernel_samples[i] = *kernel_tracks[i].sample;
        if (kernel_samples[i].num_bytes == 0) {
          syscall_failed();
        }
        total_sample_bytes += kernel_samples[i].num_bytes;
        if (total_sample_bytes > SPEAKER_MAX_SAMPLE_BYTES_TOTAL) {
          syscall_failed();
        }
        syscall_assert_userspace_buffer(kernel_samples[i].data, kernel_samples[i].num_bytes);
        kernel_tracks[i].sample = &kernel_samples[i];
      }
    }
    tracks_to_play = kernel_tracks;
  }

  if (priority > SpeakerPriorityCritical) {
    priority = SpeakerPriorityApp;
  }

  PebbleTask task = pebble_task_get_current();
  speaker_service_set_owner_task(task);

  return speaker_service_play_tracks(tracks_to_play, num_tracks,
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

DEFINE_SYSCALL(bool, sys_speaker_is_muted, void) {
  return speaker_service_is_muted();
}
