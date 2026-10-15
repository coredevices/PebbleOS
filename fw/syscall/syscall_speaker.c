/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/speaker/limits.h>
#include <pbl/services/speaker/note_sequence.h>
#include <pbl/services/speaker/speaker_pcm_format.h>
#include <pbl/services/speaker/speaker_service.h>
#include <pbl/services/speaker/track.h>

#include <kernel/pebble_tasks.h>
#include <syscall/syscall.h>
#include <syscall/syscall_define.h>
#include <system/passert.h>

#define SPEAKER_MAX_STREAM_WRITE 8192

PBL_SYSCALL(bool, sys_speaker_play_note_seq, (IN_ARRAY, const SpeakerNote *, notes, num_notes),
            (VAL, uint32_t, num_notes), (VAL, uint8_t, priority), (VAL, uint8_t, volume)) {
  if (from_user && num_notes > SPEAKER_MAX_NOTES) {
    syscall_failed();
  }

  if (from_user || priority > SpeakerPriorityCritical) {
    // Apps can only use App priority
    priority = SpeakerPriorityApp;
  }

  return speaker_service_play_note_seq_owned(notes, num_notes, (SpeakerPriority)priority, volume,
                                             pebble_task_get_current());
}

PBL_SYSCALL(bool, sys_speaker_play_tone, (VAL, uint16_t, freq_hz), (VAL, uint16_t, duration_ms),
            (VAL, uint8_t, waveform), (VAL, uint8_t, velocity), (VAL, uint8_t, priority),
            (VAL, uint8_t, volume)) {
  if (from_user || priority > SpeakerPriorityCritical) {
    priority = SpeakerPriorityApp;
  }

  if (waveform >= SpeakerWaveformCount) {
    syscall_failed();
  }

  if (velocity > 127) {
    syscall_failed();
  }

  return speaker_service_play_tone_owned(freq_hz, duration_ms, waveform, velocity,
                                         (SpeakerPriority)priority, volume,
                                         pebble_task_get_current());
}

PBL_SYSCALL(bool, sys_speaker_play_tracks, (IN_ARRAY, const SpeakerTrack *, tracks, num_tracks),
            (VAL, uint32_t, num_tracks), (VAL, uint8_t, priority), (VAL, uint8_t, volume)) {
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

  if (from_user) {
    if (num_tracks == 0 || num_tracks > SPEAKER_MAX_TRACKS) {
      syscall_failed();
    }

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
        if (kernel_samples[i].num_bytes > SPEAKER_MAX_SAMPLE_BYTES_TOTAL - total_sample_bytes) {
          syscall_failed();
        }
        total_sample_bytes += kernel_samples[i].num_bytes;
        syscall_assert_userspace_buffer(kernel_samples[i].data, kernel_samples[i].num_bytes);
        kernel_tracks[i].sample = &kernel_samples[i];
      }
    }
    tracks_to_play = kernel_tracks;
  }

  if (from_user || priority > SpeakerPriorityCritical) {
    priority = SpeakerPriorityApp;
  }

  return speaker_service_play_tracks_owned(tracks_to_play, num_tracks, (SpeakerPriority)priority,
                                           volume, pebble_task_get_current());
}

PBL_SYSCALL(bool, sys_speaker_stream_open, (VAL, uint8_t, priority), (VAL, uint8_t, volume),
            (VAL, uint8_t, format)) {
  if (from_user || priority > SpeakerPriorityCritical) {
    priority = SpeakerPriorityApp;
  }

  if (format >= SpeakerPcmFormatCount) {
    format = SpeakerPcmFormat_16kHz_16bit;
  }

  return speaker_service_stream_open_owned((SpeakerPriority)priority, volume,
                                           (SpeakerPcmFormat)format, pebble_task_get_current());
}

PBL_SYSCALL(uint32_t, sys_speaker_stream_write, (IN_BUF, const void *, data, num_bytes),
            (VAL, uint32_t, num_bytes)) {
  if (from_user) {
    if (num_bytes > 8192) {
      syscall_failed();
    }
    return speaker_service_stream_write_owned(pebble_task_get_current(), data, num_bytes);
  }

  return speaker_service_stream_write(data, num_bytes);
}

PBL_SYSCALL(void, sys_speaker_stream_close) {
  if (from_user) {
    speaker_service_stream_close_owned(pebble_task_get_current());
    return;
  }

  speaker_service_stream_close();
}

PBL_SYSCALL(void, sys_speaker_stop) {
  if (from_user) {
    speaker_service_stop_owned(pebble_task_get_current());
    return;
  }

  speaker_service_stop();
}

PBL_SYSCALL(void, sys_speaker_set_volume, (VAL, uint8_t, volume)) {
  if (from_user) {
    speaker_service_set_volume_owned(pebble_task_get_current(), volume);
    return;
  }

  speaker_service_set_volume(volume);
}

PBL_SYSCALL(uint8_t, sys_speaker_get_state) {
  return (uint8_t)speaker_service_get_state();
}

PBL_SYSCALL(void, sys_speaker_register_finish) {
  speaker_service_register_finish(pebble_task_get_current());
}

PBL_SYSCALL(bool, sys_speaker_is_muted) {
  return speaker_service_is_muted();
}
