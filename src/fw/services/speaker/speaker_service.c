/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/speaker/speaker_service.h"

#if CAPABILITY_HAS_SPEAKER

#include "pbl/services/speaker/note_sequence.h"
#include "pcm_stream.h"
#include "track_player.h"

#include "drivers/audio.h"
#include "drivers/rtc.h"
#include "board/board.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/analytics/analytics.h"
#include "pbl/services/system_task.h"
#include "system/logging.h"
#include "system/passert.h"

#include <string.h>

#define SPEAKER_SAMPLE_RATE 16000
#define SPEAKER_REFILL_SAMPLES 512

typedef struct {
  SpeakerState state;
  SpeakerSourceType source_type;
  SpeakerPriority priority;
  PebbleTask owner_task;
  uint8_t volume;

  // Note sequence source
  NoteSequenceState note_seq;
  SpeakerNote *note_buf;  // kernel_malloc'd copy of notes

  // PCM stream source
  PcmStreamState pcm_stream;
  SpeakerPcmFormat pcm_format;

  // Previous decoded samples for cubic interpolation across chunk boundaries.
  // [0] = second-to-last sample (s_{n-2}), [1] = last sample (s_{n-1}).
  int16_t prev_samples[2];

  // Temporary buffer for reading raw PCM data before format conversion
  uint8_t raw_buf[1024];

  // Refill buffer (avoids repeated small allocations)
  int16_t refill_buf[SPEAKER_REFILL_SAMPLES];

  // Polyphonic tracks source
  TrackState tracks[SPEAKER_MAX_TRACKS];
  uint32_t num_tracks;
  SpeakerNote *track_notes_buf[SPEAKER_MAX_TRACKS];
  SpeakerSample track_samples[SPEAKER_MAX_TRACKS];
  void *track_sample_data[SPEAKER_MAX_TRACKS];
  int32_t mix_buf[SPEAKER_REFILL_SAMPLES];
  int16_t track_scratch[SPEAKER_REFILL_SAMPLES];

  // Finish-event delivery
  bool finish_enabled;
  PebbleTask finish_task;

  bool initialized;
} SpeakerServiceState;

static SpeakerServiceState s_state;

//! Analytics: time-weighted average volume, reset on heartbeat.
static uint64_t s_volume_time_product_sum;     // Sum of (volume_pct × time_ms)
static RtcTicks s_last_volume_sample_ticks;    // Timestamp of last sample
static uint8_t s_last_sampled_volume_pct;      // Last volume percentage sampled
static uint32_t s_total_speaker_on_time_ms;    // Total speaker on-time tracked

static void prv_stop_internal(SpeakerFinishReason reason);
static void prv_audio_trans_cb(uint32_t *free_size);
static void prv_refill_bg(void *data);

static void prv_update_volume_analytics(uint8_t new_volume_pct) {
  RtcTicks now_ticks = rtc_get_ticks();

  if (s_last_volume_sample_ticks > 0) {
    uint32_t time_delta_ms = ((now_ticks - s_last_volume_sample_ticks) * 1000) / RTC_TICKS_HZ;

    s_volume_time_product_sum += (uint64_t)s_last_sampled_volume_pct * time_delta_ms;

    if (s_last_sampled_volume_pct > 0) {
      s_total_speaker_on_time_ms += time_delta_ms;
    }
  }

  s_last_volume_sample_ticks = now_ticks;
  s_last_sampled_volume_pct = new_volume_pct;
}

void speaker_service_init(void) {
  memset(&s_state, 0, sizeof(s_state));
  s_state.state = SpeakerStateIdle;
  s_state.source_type = SpeakerSourceNone;
  s_state.owner_task = PebbleTask_Unknown;
  s_state.initialized = true;

  s_volume_time_product_sum = 0;
  s_last_volume_sample_ticks = 0;
  s_last_sampled_volume_pct = 0;
  s_total_speaker_on_time_ms = 0;
}

static void prv_start_audio(uint8_t vol) {
  PBL_ANALYTICS_TIMER_START(speaker_on_time_ms);
  PBL_ANALYTICS_ADD(speaker_play_count, 1);
  prv_update_volume_analytics(vol);

  audio_init((AudioDevice *)AUDIO);
  audio_set_volume((AudioDevice *)AUDIO, vol);
  audio_start((AudioDevice *)AUDIO, prv_audio_trans_cb);
}

static void prv_stop_audio(void) {
  PBL_ANALYTICS_TIMER_STOP(speaker_on_time_ms);
  prv_update_volume_analytics(0);

  audio_stop((AudioDevice *)AUDIO);
}

static void prv_free_tracks(void) {
  for (uint32_t i = 0; i < s_state.num_tracks; i++) {
    track_deinit(&s_state.tracks[i]);
    if (s_state.track_notes_buf[i]) {
      kernel_free(s_state.track_notes_buf[i]);
      s_state.track_notes_buf[i] = NULL;
    }
    if (s_state.track_sample_data[i]) {
      kernel_free(s_state.track_sample_data[i]);
      s_state.track_sample_data[i] = NULL;
    }
  }
  s_state.num_tracks = 0;
}

static void prv_post_finish_event(SpeakerFinishReason reason) {
  if (!s_state.finish_enabled) {
    return;
  }
  PebbleEvent e = {
    .type = PEBBLE_SPEAKER_EVENT,
    .speaker = {
      .type = SpeakerEventFinished,
      .finish_reason = (uint8_t)reason,
    },
  };
  event_put(&e);
}

static void prv_stop_internal(SpeakerFinishReason reason) {
  if (s_state.state == SpeakerStateIdle) {
    return;
  }

  if (reason == SpeakerFinishReasonPreempted) {
    PBL_ANALYTICS_ADD(speaker_preempted_count, 1);
  }

  prv_stop_audio();

  if (s_state.source_type == SpeakerSourceNoteSeq) {
    note_seq_deinit(&s_state.note_seq);
    if (s_state.note_buf) {
      kernel_free(s_state.note_buf);
      s_state.note_buf = NULL;
    }
  } else if (s_state.source_type == SpeakerSourceStream) {
    pcm_stream_deinit(&s_state.pcm_stream);
  } else if (s_state.source_type == SpeakerSourceTracks) {
    prv_free_tracks();
  }

  s_state.state = SpeakerStateIdle;
  s_state.source_type = SpeakerSourceNone;
  s_state.owner_task = PebbleTask_Unknown;

  PBL_LOG_DBG("Speaker stopped (reason=%d)", reason);

  prv_post_finish_event(reason);
}

static bool prv_can_preempt(SpeakerPriority new_pri) {
  if (s_state.state == SpeakerStateIdle) {
    return true;
  }
  return new_pri > s_state.priority;
}

//! Called from system task context when the audio driver needs more data.
//! This is the DMA refill callback path:
//!   DMA ISR -> system_task_add_callback_from_isr -> audio driver trans_cb -> here
static void prv_audio_trans_cb(uint32_t *free_size) {
  // Schedule actual refill work on system task to keep ISR-context callback short
  system_task_add_callback(prv_refill_bg, NULL);
}

//! Convert a raw sample from the input buffer to 16-bit signed.
static int16_t prv_decode_sample(const uint8_t *raw, uint32_t index, bool is_16bit) {
  if (is_16bit) {
    return (int16_t)(raw[index * 2] | (raw[index * 2 + 1] << 8));
  }
  // 8-bit signed -> 16-bit signed: scale [-128,127] to [-32768,32512]
  return (int16_t)((int8_t)raw[index]) << 8;
}

//! 4-tap cubic midpoint interpolation for 2x upsampling.
//! Uses the halfband polyphase filter: (-s0 + 9*s1 + 9*s2 - s3) / 16
//! This produces a smooth curve through all original sample points with
//! continuous first derivatives, unlike linear interpolation.
static inline int16_t prv_cubic_midpoint(int16_t s0, int16_t s1, int16_t s2, int16_t s3) {
  int32_t v = -(int32_t)s0 + 9 * (int32_t)s1 + 9 * (int32_t)s2 - (int32_t)s3;
  // Clamp to int16_t range before dividing
  v = (v + 8) >> 4;  // divide by 16 with rounding
  if (v > 32767) v = 32767;
  if (v < -32768) v = -32768;
  return (int16_t)v;
}

//! Read raw PCM data from the stream and convert to 16kHz 16-bit output.
//! For 8kHz input, uses 4-tap cubic interpolation between consecutive samples
//! for smooth upsampling without staircase artifacts. Maintains state across
//! refill calls via prev_samples[] for seamless chunk boundaries.
//! @param out Output buffer for 16-bit samples
//! @param max_out_samples Maximum number of output samples to produce
//! @return Number of output samples written
static uint32_t prv_read_and_convert_pcm(int16_t *out, uint32_t max_out_samples) {
  SpeakerPcmFormat fmt = s_state.pcm_format;
  bool is_16khz = (fmt & 1);
  bool is_16bit = (fmt & 2);

  // Calculate how many input bytes we need for max_out_samples output samples
  // Output is always 16kHz 16-bit
  uint32_t input_samples_needed = is_16khz ? max_out_samples : (max_out_samples / 2);
  uint32_t bytes_per_sample = is_16bit ? 2 : 1;
  uint32_t input_bytes_needed = input_samples_needed * bytes_per_sample;

  // Clamp to raw_buf size
  if (input_bytes_needed > sizeof(s_state.raw_buf)) {
    input_bytes_needed = sizeof(s_state.raw_buf);
    input_samples_needed = input_bytes_needed / bytes_per_sample;
  }

  uint32_t bytes_read = pcm_stream_read(&s_state.pcm_stream, s_state.raw_buf,
                                         input_bytes_needed);
  if (bytes_read == 0) {
    return 0;
  }

  uint32_t samples_read = bytes_read / bytes_per_sample;
  uint32_t out_pos = 0;

  if (is_16khz) {
    // No upsampling needed — just convert bit depth
    for (uint32_t i = 0; i < samples_read; i++) {
      out[out_pos++] = prv_decode_sample(s_state.raw_buf, i, is_16bit);
    }
    // Track last two samples for potential future use
    if (samples_read >= 2) {
      s_state.prev_samples[0] = out[out_pos - 2];
      s_state.prev_samples[1] = out[out_pos - 1];
    } else if (samples_read == 1) {
      s_state.prev_samples[0] = s_state.prev_samples[1];
      s_state.prev_samples[1] = out[out_pos - 1];
    }
  } else {
    // 8kHz -> 16kHz: 4-tap cubic interpolation
    // For each input sample, output the sample itself plus a cubic-interpolated
    // midpoint using 4 surrounding points: s[i-1], s[i], s[i+1], s[i+2]
    // prev_samples[] provides the history across chunk boundaries.
    for (uint32_t i = 0; i < samples_read; i++) {
      int16_t s_prev = (i >= 1) ? prv_decode_sample(s_state.raw_buf, i - 1, is_16bit)
                                : s_state.prev_samples[1];
      int16_t s_curr = prv_decode_sample(s_state.raw_buf, i, is_16bit);
      int16_t s_next = (i + 1 < samples_read)
                            ? prv_decode_sample(s_state.raw_buf, i + 1, is_16bit)
                            : s_curr;
      int16_t s_next2 = (i + 2 < samples_read)
                             ? prv_decode_sample(s_state.raw_buf, i + 2, is_16bit)
                             : s_next;

      out[out_pos++] = s_curr;
      out[out_pos++] = prv_cubic_midpoint(s_prev, s_curr, s_next, s_next2);
    }

    // Save last two decoded samples for next chunk's interpolation
    if (samples_read >= 2) {
      s_state.prev_samples[0] = prv_decode_sample(s_state.raw_buf, samples_read - 2,
                                                   is_16bit);
      s_state.prev_samples[1] = prv_decode_sample(s_state.raw_buf, samples_read - 1,
                                                   is_16bit);
    } else if (samples_read == 1) {
      s_state.prev_samples[0] = s_state.prev_samples[1];
      s_state.prev_samples[1] = prv_decode_sample(s_state.raw_buf, 0, is_16bit);
    }
  }

  return out_pos;
}

static void prv_refill_bg(void *data) {
  if (s_state.state == SpeakerStateIdle) {
    return;
  }

  uint32_t samples_generated = 0;

  if (s_state.source_type == SpeakerSourceNoteSeq) {
    samples_generated = note_seq_fill(&s_state.note_seq, s_state.refill_buf,
                                      SPEAKER_REFILL_SAMPLES);
    if (samples_generated == 0) {
      prv_stop_internal(SpeakerFinishReasonDone);
      return;
    }
  } else if (s_state.source_type == SpeakerSourceStream) {
    samples_generated = prv_read_and_convert_pcm(s_state.refill_buf,
                                                  SPEAKER_REFILL_SAMPLES);
    if (samples_generated == 0 && pcm_stream_is_done(&s_state.pcm_stream)) {
      prv_stop_internal(SpeakerFinishReasonDone);
      return;
    }
    // If no data but not done, write silence to keep DMA fed
    if (samples_generated == 0) {
      PBL_ANALYTICS_ADD(speaker_stream_underrun_count, 1);
      memset(s_state.refill_buf, 0, SPEAKER_REFILL_SAMPLES * sizeof(int16_t));
      samples_generated = SPEAKER_REFILL_SAMPLES;
    }
  } else if (s_state.source_type == SpeakerSourceTracks) {
    memset(s_state.mix_buf, 0, sizeof(int32_t) * SPEAKER_REFILL_SAMPLES);
    uint32_t max_generated = 0;
    for (uint32_t i = 0; i < s_state.num_tracks; i++) {
      uint32_t n = track_fill(&s_state.tracks[i], s_state.track_scratch,
                              SPEAKER_REFILL_SAMPLES);
      for (uint32_t j = 0; j < n; j++) {
        s_state.mix_buf[j] += s_state.track_scratch[j];
      }
      if (n > max_generated) {
        max_generated = n;
      }
    }
    if (max_generated == 0) {
      prv_stop_internal(SpeakerFinishReasonDone);
      return;
    }
    for (uint32_t j = 0; j < max_generated; j++) {
      int32_t v = s_state.mix_buf[j];
      if (v > 32767) v = 32767;
      else if (v < -32768) v = -32768;
      s_state.refill_buf[j] = (int16_t)v;
    }
    samples_generated = max_generated;
  }

  if (samples_generated > 0) {
    audio_write((AudioDevice *)AUDIO, s_state.refill_buf,
                samples_generated * sizeof(int16_t));
  }
}

bool speaker_service_play_note_seq(const SpeakerNote *notes, uint32_t num_notes,
                                   SpeakerPriority pri, uint8_t vol) {
  if (!s_state.initialized || !notes || num_notes == 0) {
    return false;
  }

  if (!prv_can_preempt(pri)) {
    return false;
  }

  // Stop any existing playback
  if (s_state.state != SpeakerStateIdle) {
    prv_stop_internal(SpeakerFinishReasonPreempted);
  }

  // Copy notes to kernel memory so they persist
  size_t notes_size = num_notes * sizeof(SpeakerNote);
  s_state.note_buf = kernel_malloc(notes_size);
  if (!s_state.note_buf) {
    PBL_LOG_ERR("Failed to allocate note buffer");
    return false;
  }
  memcpy(s_state.note_buf, notes, notes_size);

  note_seq_init(&s_state.note_seq, s_state.note_buf, num_notes, SPEAKER_SAMPLE_RATE);

  s_state.state = SpeakerStatePlaying;
  s_state.source_type = SpeakerSourceNoteSeq;
  s_state.priority = pri;
  s_state.volume = vol;

  prv_start_audio(vol);

  // Prime the audio buffer with initial data
  prv_refill_bg(NULL);

  return true;
}

bool speaker_service_play_tracks(const SpeakerTrack *tracks, uint32_t num_tracks,
                                 SpeakerPriority pri, uint8_t vol) {
  if (!s_state.initialized || !tracks || num_tracks == 0 ||
      num_tracks > SPEAKER_MAX_TRACKS) {
    return false;
  }

  uint32_t total_sample_bytes = 0;
  for (uint32_t i = 0; i < num_tracks; i++) {
    const SpeakerTrack *t = &tracks[i];
    if (!t->notes || t->num_notes == 0) {
      return false;
    }
    if (t->sample) {
      if (!t->sample->data || t->sample->num_bytes == 0) {
        return false;
      }
      total_sample_bytes += t->sample->num_bytes;
      if (total_sample_bytes > SPEAKER_MAX_SAMPLE_BYTES_TOTAL) {
        return false;
      }
    }
  }

  if (!prv_can_preempt(pri)) {
    return false;
  }

  if (s_state.state != SpeakerStateIdle) {
    prv_stop_internal(SpeakerFinishReasonPreempted);
  }

  // Kernel-copy each track's notes, sample struct, and sample data. Build a
  // local array of SpeakerTrack pointing at the kernel copies.
  SpeakerTrack kernel_tracks[SPEAKER_MAX_TRACKS];
  memset(kernel_tracks, 0, sizeof(kernel_tracks));

  for (uint32_t i = 0; i < num_tracks; i++) {
    const SpeakerTrack *t = &tracks[i];
    size_t notes_size = (size_t)t->num_notes * sizeof(SpeakerNote);
    s_state.track_notes_buf[i] = kernel_malloc(notes_size);
    if (!s_state.track_notes_buf[i]) {
      PBL_LOG_ERR("Failed to allocate track notes");
      goto alloc_fail;
    }
    memcpy(s_state.track_notes_buf[i], t->notes, notes_size);

    kernel_tracks[i].notes = s_state.track_notes_buf[i];
    kernel_tracks[i].num_notes = t->num_notes;

    if (t->sample) {
      s_state.track_sample_data[i] = kernel_malloc(t->sample->num_bytes);
      if (!s_state.track_sample_data[i]) {
        PBL_LOG_ERR("Failed to allocate sample data");
        goto alloc_fail;
      }
      memcpy(s_state.track_sample_data[i], t->sample->data, t->sample->num_bytes);

      s_state.track_samples[i] = *t->sample;
      s_state.track_samples[i].data = s_state.track_sample_data[i];
      kernel_tracks[i].sample = &s_state.track_samples[i];
    }
  }

  s_state.num_tracks = num_tracks;
  for (uint32_t i = 0; i < num_tracks; i++) {
    track_init(&s_state.tracks[i], &kernel_tracks[i], SPEAKER_SAMPLE_RATE);
  }

  s_state.state = SpeakerStatePlaying;
  s_state.source_type = SpeakerSourceTracks;
  s_state.priority = pri;
  s_state.volume = vol;

  prv_start_audio(vol);
  prv_refill_bg(NULL);

  return true;

alloc_fail:
  for (uint32_t i = 0; i < num_tracks; i++) {
    if (s_state.track_notes_buf[i]) {
      kernel_free(s_state.track_notes_buf[i]);
      s_state.track_notes_buf[i] = NULL;
    }
    if (s_state.track_sample_data[i]) {
      kernel_free(s_state.track_sample_data[i]);
      s_state.track_sample_data[i] = NULL;
    }
  }
  return false;
}

bool speaker_service_stream_open(SpeakerPriority pri, uint8_t vol, SpeakerPcmFormat fmt) {
  if (!s_state.initialized) {
    return false;
  }

  if (!prv_can_preempt(pri)) {
    return false;
  }

  // Stop any existing playback
  if (s_state.state != SpeakerStateIdle) {
    prv_stop_internal(SpeakerFinishReasonPreempted);
  }

  if (!pcm_stream_init(&s_state.pcm_stream, PCM_STREAM_DEFAULT_SIZE_BYTES)) {
    PBL_LOG_ERR("Failed to allocate PCM stream buffer");
    return false;
  }

  s_state.state = SpeakerStatePlaying;
  s_state.source_type = SpeakerSourceStream;
  s_state.priority = pri;
  s_state.volume = vol;
  s_state.pcm_format = fmt;
  s_state.prev_samples[0] = 0;
  s_state.prev_samples[1] = 0;

  prv_start_audio(vol);

  return true;
}

uint32_t speaker_service_stream_write(const void *data, uint32_t num_bytes) {
  if (s_state.state == SpeakerStateIdle ||
      s_state.source_type != SpeakerSourceStream) {
    return 0;
  }

  return pcm_stream_write(&s_state.pcm_stream, data, num_bytes);
}

void speaker_service_stream_close(void) {
  if (s_state.source_type != SpeakerSourceStream) {
    return;
  }

  if (s_state.pcm_stream.count > 0) {
    // Data remaining - enter draining state
    pcm_stream_mark_closing(&s_state.pcm_stream);
    s_state.state = SpeakerStateDraining;
  } else {
    prv_stop_internal(SpeakerFinishReasonDone);
  }
}

void speaker_service_stop(void) {
  prv_stop_internal(SpeakerFinishReasonStopped);
}

void speaker_service_set_volume(uint8_t vol) {
  s_state.volume = vol;
  if (s_state.state != SpeakerStateIdle) {
    prv_update_volume_analytics(vol);
    audio_set_volume((AudioDevice *)AUDIO, vol);
  }
}

SpeakerState speaker_service_get_state(void) {
  return s_state.state;
}

void speaker_service_stop_for_task(PebbleTask task) {
  if (s_state.state != SpeakerStateIdle && s_state.owner_task == task) {
    // App is going away — no one to receive the finish event.
    s_state.finish_enabled = false;
    prv_stop_internal(SpeakerFinishReasonStopped);
  }
  if (s_state.finish_task == task) {
    s_state.finish_enabled = false;
    s_state.finish_task = PebbleTask_Unknown;
  }
}

void speaker_service_set_owner_task(PebbleTask task) {
  s_state.owner_task = task;
}

void speaker_service_register_finish(PebbleTask task) {
  s_state.finish_enabled = true;
  s_state.finish_task = task;
}

void pbl_analytics_external_collect_speaker_stats(void) {
  // Capture one final sample to account for time since last volume change.
  prv_update_volume_analytics(s_last_sampled_volume_pct);

  uint32_t avg_volume_pct = 0;
  if (s_total_speaker_on_time_ms > 0) {
    avg_volume_pct = s_volume_time_product_sum / s_total_speaker_on_time_ms;
  }

  PBL_ANALYTICS_SET_UNSIGNED(speaker_avg_volume_pct, avg_volume_pct);

  s_volume_time_product_sum = 0;
  s_total_speaker_on_time_ms = 0;
  s_last_volume_sample_ticks = rtc_get_ticks();
}

#else // !CAPABILITY_HAS_SPEAKER

void speaker_service_init(void) {}

bool speaker_service_play_note_seq(const SpeakerNote *notes, uint32_t num_notes,
                                   SpeakerPriority pri, uint8_t vol) {
  return false;
}

bool speaker_service_play_tracks(const SpeakerTrack *tracks, uint32_t num_tracks,
                                 SpeakerPriority pri, uint8_t vol) {
  return false;
}

bool speaker_service_stream_open(SpeakerPriority pri, uint8_t vol, SpeakerPcmFormat fmt) {
  return false;
}

uint32_t speaker_service_stream_write(const void *data, uint32_t num_bytes) {
  return 0;
}

void speaker_service_stream_close(void) {}
void speaker_service_stop(void) {}
void speaker_service_set_volume(uint8_t vol) {}

SpeakerState speaker_service_get_state(void) {
  return SpeakerStateIdle;
}

void speaker_service_stop_for_task(PebbleTask task) {}
void speaker_service_set_owner_task(PebbleTask task) {}
void speaker_service_register_finish(PebbleTask task) {}
void pbl_analytics_external_collect_speaker_stats(void) {}

#endif // CAPABILITY_HAS_SPEAKER
