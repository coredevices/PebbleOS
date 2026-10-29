/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/speaker/speaker_service.h>

#ifdef CONFIG_SPEAKER

#include "pcm_stream.h"
#include "track_player.h"

#include <errno.h>
#include <string.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/battery.h>
#include <pbl/drivers/rtc.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/services/analytics/analytics.h>
#include <pbl/services/notifications/alerts_preferences.h>
#include <pbl/services/notifications/do_not_disturb.h>
#include <pbl/services/speaker/note_sequence.h>
#include <pbl/services/system_task.h>
#include <pbl/util/circular_buffer.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#include <board/board.h>
#include <kernel/events.h>
#include <kernel/pbl_malloc.h>

PBL_LOG_MODULE_DEFINE(service_speaker, CONFIG_SERVICE_SPEAKER_LOG_LEVEL);

#define SPEAKER_SAMPLE_RATE    16000
#define SPEAKER_REFILL_SAMPLES 512
#define MS_PER_S               1000

// Device ring: two periods of one refill each
#define SPEAKER_PERIOD_FRAMES SPEAKER_REFILL_SAMPLES
#define SPEAKER_PERIODS       2

// Samples queued between the synthesizer on KernelBG and the device ring
#define SPEAKER_FIFO_MS    128
#define SPEAKER_FIFO_BYTES ((SPEAKER_SAMPLE_RATE * SPEAKER_FIFO_MS / MS_PER_S) * sizeof(int16_t))

// Silent periods the device keeps streaming after playback before it stops
#define SPEAKER_STANDBY_PERIODS                                                  \
  DIVIDE_CEIL(CONFIG_SERVICE_SPEAKER_STANDBY_MS *SPEAKER_SAMPLE_RATE / MS_PER_S, \
              SPEAKER_PERIOD_FRAMES)

// Volume 1 sits this far below volume 100, or at the lowest gain of the device if it is closer
#define SPEAKER_VOLUME_FLOOR_MDB (-48000)
#define SPEAKER_VOLUME_MAX       100

// Volume-preview tone: square, to match the perceived loudness
#define VOLUME_PREVIEW_FREQ_HZ     660
#define VOLUME_PREVIEW_DURATION_MS 200

typedef struct {
  SpeakerState state;
  SpeakerSourceType source_type;
  SpeakerPriority priority;
  PebbleTask owner_task;
  uint8_t volume;
  bool volume_absolute;

  // Note sequence source
  NoteSequenceState note_seq;
  SpeakerNote *note_buf; // kernel_malloc'd copy of notes

  // Single tone source (raw frequency, no MIDI quantization)
  uint32_t tone_samples_remaining;
  uint32_t tone_phase_acc; // 16.16 fixed-point
  uint32_t tone_phase_inc; // per-sample phase increment
  uint8_t tone_waveform;
  uint8_t tone_velocity;

  // PCM stream source
  PcmStreamState pcm_stream;
  SpeakerPcmFormat pcm_format;
  bool stream_realtime;

  // Cubic interpolation history, oldest first; two input samples of delay.
  int16_t prev_samples[3];
  uint8_t pcm_tail_samples;

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

  // Set once the source has no more samples; playback ends when the device has played the
  // queued samples.
  bool pipeline_draining;

  // Device stream: ring of periods and the queue the period callback drains
  bool device_running;
  int16_t *ring;
  uint8_t *fifo_storage;
  CircularBuffer fifo;
  // Periods handed out with no queued samples, counted from the device interrupt
  volatile uint32_t idle_periods;
  volatile bool bottom_half_pending;

  // Finish-event delivery
  bool finish_enabled;
  PebbleTask finish_task;

  bool initialized;
} SpeakerServiceState;

static SpeakerServiceState s_state;

// Serializes producers and driver refill callbacks.
static PBL_MUTEX_DEFINE(s_lock);

//! Why playback is currently silent, cached so a muted watch logs once per change
//! rather than on every sound.
#define SPEAKER_SILENT         (1 << 0)
#define SPEAKER_SILENT_BY_MUTE (1 << 1)
#define SPEAKER_SILENT_BY_DND  (1 << 2)
static uint8_t s_silence_reasons;

//! Analytics: time-weighted average volume, reset on heartbeat.
static uint64_t s_volume_time_product_sum;  // Sum of (volume_pct × time_ms)
static RtcTicks s_last_volume_sample_ticks; // Timestamp of last sample
static uint8_t s_last_sampled_volume_pct;   // Last volume percentage sampled
static uint32_t s_total_speaker_on_time_ms; // Total speaker on-time tracked

static void prv_stop_internal(SpeakerFinishReason reason);
static void prv_refill_locked(void);
static void prv_refill_realtime_locked(void);

//! Quiet Time does not silence critical sounds (alarms), like it does not stop their vibration.
static bool prv_is_muted_by_quiet_time(SpeakerPriority pri) {
  return pri < SpeakerPriorityCritical && alerts_preferences_dnd_get_mute_speaker() &&
         do_not_disturb_is_active();
}

static bool prv_is_speaker_muted(SpeakerPriority pri) {
  return alerts_preferences_get_speaker_muted() || prv_is_muted_by_quiet_time(pri);
}

static uint8_t prv_effective_volume(uint8_t vol) {
  if (prv_is_speaker_muted(s_state.priority)) {
    return 0;
  }
  if (s_state.volume_absolute) {
    return vol;
  }
  const uint8_t cap = alerts_preferences_get_speaker_volume();
  return (uint32_t)vol * cap / 100;
}

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

  s_silence_reasons = 0;
  s_volume_time_product_sum = 0;
  s_last_volume_sample_ticks = 0;
  s_last_sampled_volume_pct = 0;
  s_total_speaker_on_time_ms = 0;
}

//! Silent playback looks identical to broken hardware from the outside, so record
//! which setting silenced it. Logs only when the reason changes: this runs on every
//! sound, and a muted watch would otherwise flood the log.
static void prv_log_silence(uint8_t vol, uint8_t effective_vol) {
  uint8_t reasons = 0;

  if (effective_vol == 0) {
    reasons = SPEAKER_SILENT;
    if (alerts_preferences_get_speaker_muted()) {
      reasons |= SPEAKER_SILENT_BY_MUTE;
    }
    if (prv_is_muted_by_quiet_time(s_state.priority)) {
      reasons |= SPEAKER_SILENT_BY_DND;
    }
  }

  if (reasons == s_silence_reasons) {
    return;
  }
  s_silence_reasons = reasons;

  if (reasons != 0) {
    PBL_LOG_INFO("Playing silently: vol=%" PRIu8 " cap=%" PRIu8 " mute=%d quiet_time_mute=%d", vol,
                 alerts_preferences_get_speaker_volume(), (reasons & SPEAKER_SILENT_BY_MUTE) != 0,
                 (reasons & SPEAKER_SILENT_BY_DND) != 0);
  }
}

static uint16_t prv_fifo_free(void) {
  pbl_irq_lock();
  uint16_t free = circular_buffer_get_write_space_remaining(&s_state.fifo);
  pbl_irq_unlock();
  return free;
}

static uint16_t prv_fifo_used(void) {
  pbl_irq_lock();
  uint16_t used = circular_buffer_get_read_space_remaining(&s_state.fifo);
  pbl_irq_unlock();
  return used;
}

static void prv_fifo_write(const int16_t *samples, uint32_t count) {
  pbl_irq_lock();
  (void)circular_buffer_write(&s_state.fifo, samples, count * sizeof(int16_t));
  s_state.idle_periods = 0;
  pbl_irq_unlock();
}

static void prv_fifo_flush(void) {
  pbl_irq_lock();
  circular_buffer_consume(&s_state.fifo, circular_buffer_get_read_space_remaining(&s_state.fifo));
  pbl_irq_unlock();
}

static void prv_bottom_half(void *data);

//! Runs from the device interrupt: drain the queue into the period and wake KernelBG once.
static void prv_period_done(const struct pbl_audio *audio, void *period,
                            const struct pbl_audio_pos *pos, void *arg) {
  uint16_t avail = circular_buffer_get_read_space_remaining(&s_state.fifo);
  uint16_t bytes = MIN(avail, SPEAKER_PERIOD_FRAMES * sizeof(int16_t));

  if (bytes > 0) {
    (void)circular_buffer_copy(&s_state.fifo, period, bytes);
    circular_buffer_consume(&s_state.fifo, bytes);
    s_state.idle_periods = 0;
  } else {
    s_state.idle_periods++;
  }

  (void)pbl_audio_period_ready(audio, period);

  // A dropped wakeup is retried on the next period; underrunning beats flooding the queue
  if (!s_state.bottom_half_pending) {
    s_state.bottom_half_pending = true;
    if (!system_task_add_callback_from_isr_droppable_raised(prv_bottom_half, nullptr)) {
      s_state.bottom_half_pending = false;
    }
  }
}

//! The speaker draws more than the default battery discharge limit allows
static void prv_high_load(bool on) {
#ifdef CONFIG_BATTERY_HIGH_LOAD
  if (on) {
    (void)battery_high_load_request();
  } else {
    (void)battery_high_load_release();
  }
#endif
}

static void prv_device_down(void) {
  if (!s_state.device_running) {
    return;
  }

  (void)pbl_audio_stop(SPEAKER);
  prv_high_load(false);
  pbl_audio_buf_free(s_state.ring);
  kernel_free(s_state.fifo_storage);
  s_state.ring = nullptr;
  s_state.fifo_storage = nullptr;
  s_state.device_running = false;
}

static int prv_device_up(void) {
  struct pbl_audio_stream stream = {
    .rate = SPEAKER_SAMPLE_RATE,
    .channels = 1,
    .period_frames = SPEAKER_PERIOD_FRAMES,
    .periods = SPEAKER_PERIODS,
    .period_done = prv_period_done,
  };
  int ret;

  if (s_state.device_running) {
    prv_fifo_flush();
    return 0;
  }

  s_state.fifo_storage = kernel_malloc(SPEAKER_FIFO_BYTES);
  stream.buf = pbl_audio_buf_alloc(pbl_audio_ring_bytes(&stream));
  if (s_state.fifo_storage == nullptr || stream.buf == nullptr) {
    kernel_free(s_state.fifo_storage);
    pbl_audio_buf_free(stream.buf);
    s_state.fifo_storage = nullptr;
    return -ENOMEM;
  }

  circular_buffer_init(&s_state.fifo, s_state.fifo_storage, SPEAKER_FIFO_BYTES);
  s_state.ring = stream.buf;
  s_state.idle_periods = 0;
  s_state.bottom_half_pending = false;

  prv_high_load(true);
  ret = pbl_audio_start(SPEAKER, &stream);
  if (ret != 0) {
    prv_high_load(false);
    pbl_audio_buf_free(s_state.ring);
    kernel_free(s_state.fifo_storage);
    s_state.ring = nullptr;
    s_state.fifo_storage = nullptr;
    return ret;
  }

  s_state.device_running = true;

  return 0;
}

//! Maps volume 1-100 onto the gain steps between the floor and 0 dB, never above 0 dB.
static void prv_apply_volume(uint8_t vol) {
  struct pbl_audio_caps caps;

  if (vol == 0) {
    (void)pbl_audio_set_mute(SPEAKER, true);
    return;
  }

  if (pbl_audio_get_caps(SPEAKER, &caps) == 0 && caps.gain_step_mdb > 0) {
    const int32_t top = MIN(caps.gain_max_mdb, 0);
    const int32_t floor = MAX(caps.gain_min_mdb, SPEAKER_VOLUME_FLOOR_MDB);
    const int32_t span_steps = (top - floor) / caps.gain_step_mdb;
    const int32_t steps = (SPEAKER_VOLUME_MAX - (int32_t)vol) * span_steps / SPEAKER_VOLUME_MAX;

    (void)pbl_audio_set_gain(SPEAKER, top - steps * caps.gain_step_mdb);
  }

  (void)pbl_audio_set_mute(SPEAKER, false);
}

static bool prv_start_audio(uint8_t vol) {
  const uint8_t effective_vol = prv_effective_volume(vol);
  int ret;

  prv_log_silence(vol, effective_vol);
  prv_apply_volume(effective_vol);

  ret = prv_device_up();
  if (ret != 0) {
    PBL_LOG_WRN("Speaker failed to start (%d)", ret);
    return false;
  }

  PBL_ANALYTICS_TIMER_START(speaker_on_time_ms);
  PBL_ANALYTICS_ADD(speaker_play_count, 1);
  prv_update_volume_analytics(effective_vol);

  return true;
}

static void prv_stop_audio(void) {
  PBL_ANALYTICS_TIMER_STOP(speaker_on_time_ms);
  prv_update_volume_analytics(0);

  prv_fifo_flush();
  if (SPEAKER_STANDBY_PERIODS == 0) {
    prv_device_down();
  }
}

static void prv_free_tracks(void) {
  for (uint32_t i = 0; i < s_state.num_tracks; i++) {
    track_deinit(&s_state.tracks[i]);
    if (s_state.track_notes_buf[i]) {
      kernel_free(s_state.track_notes_buf[i]);
      s_state.track_notes_buf[i] = nullptr;
    }
    if (s_state.track_sample_data[i]) {
      kernel_free(s_state.track_sample_data[i]);
      s_state.track_sample_data[i] = nullptr;
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

//! Caller must hold s_lock. PebbleTask_Unknown keeps the current owner.
static void prv_set_owner_locked(PebbleTask owner) {
  if (owner != PebbleTask_Unknown) {
    s_state.owner_task = owner;
  }
}

//! Caller must hold s_lock.
static void prv_stop_internal(SpeakerFinishReason reason) {
  if (s_state.state == SpeakerStateIdle) {
    return;
  }
  const SpeakerSourceType source_type = s_state.source_type;
  s_state.state = SpeakerStateIdle;
  s_state.source_type = SpeakerSourceNone;
  s_state.owner_task = PebbleTask_Unknown;
  s_state.pipeline_draining = false;
  s_state.volume_absolute = false;

  if (reason == SpeakerFinishReasonPreempted) {
    PBL_ANALYTICS_ADD(speaker_preempted_count, 1);
  }

  prv_stop_audio();

  if (source_type == SpeakerSourceNoteSeq) {
    note_seq_deinit(&s_state.note_seq);
    if (s_state.note_buf) {
      kernel_free(s_state.note_buf);
      s_state.note_buf = nullptr;
    }
  } else if (source_type == SpeakerSourceStream) {
    pcm_stream_deinit(&s_state.pcm_stream);
  } else if (source_type == SpeakerSourceTracks) {
    prv_free_tracks();
  } else if (source_type == SpeakerSourceTone) {
    s_state.tone_samples_remaining = 0;
    s_state.tone_phase_inc = 0;
  }

  PBL_LOG_DBG("Speaker stopped (reason=%d)", reason);

  prv_post_finish_event(reason);
}

//! Caller must hold s_lock. Starts the device for the source just set up, or drops the source
//! without a finish event.
static bool prv_start_or_abort_locked(uint8_t vol) {
  if (prv_start_audio(vol)) {
    return true;
  }

  const bool finish_enabled = s_state.finish_enabled;
  s_state.finish_enabled = false;
  prv_stop_internal(SpeakerFinishReasonStopped);
  s_state.finish_enabled = finish_enabled;
  return false;
}

static bool prv_can_preempt(SpeakerPriority new_pri) {
  if (s_state.state == SpeakerStateIdle) {
    return true;
  }
  if (s_state.pipeline_draining) {
    // Only padding silence remains — let anyone start a new sound rather
    // than rejecting it (matters for rapid re-triggers, e.g. a metronome).
    return true;
  }
  return new_pri > s_state.priority;
}

//! KernelBG side of the device interrupt: refill the queue, end drained playback and stop the
//! device once it has idled through its standby.
static void prv_bottom_half(void *data) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_state.bottom_half_pending = false;

  if (s_state.source_type == SpeakerSourceStream && s_state.stream_realtime) {
    prv_refill_realtime_locked();
  } else {
    // Catch up queued PCM without inserting silence between short packets.
    uint32_t refill_count = s_state.source_type == SpeakerSourceStream
                                ? prv_fifo_free() / sizeof(s_state.refill_buf)
                                : (prv_fifo_free() >= sizeof(s_state.refill_buf) ? 1 : 0);
    while (refill_count-- && s_state.state != SpeakerStateIdle && !s_state.pipeline_draining) {
      prv_refill_locked();
      if (s_state.source_type == SpeakerSourceStream &&
          pcm_stream_available(&s_state.pcm_stream) == 0) {
        break;
      }
    }
  }

  if (s_state.pipeline_draining && prv_fifo_used() == 0 &&
      s_state.idle_periods >= SPEAKER_PERIODS) {
    prv_stop_internal(SpeakerFinishReasonDone);
  }

  if (s_state.state == SpeakerStateIdle && s_state.idle_periods + 1 > SPEAKER_STANDBY_PERIODS) {
    prv_device_down();
  }

  pbl_mutex_unlock(&s_lock);
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
  v = (v + 8) >> 4; // divide by 16 with rounding
  if (v > 32767)
    v = 32767;
  if (v < -32768)
    v = -32768;
  return (int16_t)v;
}

static uint32_t prv_pcm_bytes_per_sample(void) {
  return (s_state.pcm_format & 2) ? 2 : 1;
}

//! Input samples that fit both max_out_samples of output and raw_buf.
static uint32_t prv_pcm_input_samples(uint32_t max_out_samples) {
  bool is_16khz = (s_state.pcm_format & 1);
  uint32_t needed = is_16khz ? max_out_samples : (max_out_samples / 2);
  return MIN(needed, sizeof(s_state.raw_buf) / prv_pcm_bytes_per_sample());
}

//! Convert num_samples input samples (zeros when raw is NULL) to 16kHz 16-bit output.
//! For 8kHz input, uses 4-tap cubic interpolation between consecutive samples
//! for smooth upsampling without staircase artifacts. Maintains state across
//! calls via prev_samples[] for seamless chunk boundaries.
//! @return Number of output samples written
static uint32_t prv_convert_pcm(int16_t *out, const uint8_t *raw, uint32_t num_samples) {
  bool is_16khz = (s_state.pcm_format & 1);
  bool is_16bit = (s_state.pcm_format & 2);
  uint32_t out_pos = 0;

  for (uint32_t i = 0; i < num_samples; i++) {
    int16_t sample = raw ? prv_decode_sample(raw, i, is_16bit) : 0;
    if (is_16khz) {
      out[out_pos++] = sample;
      continue;
    }
    // Delay output until all four taps exist, including across short reads.
    out[out_pos++] = s_state.prev_samples[1];
    out[out_pos++] = prv_cubic_midpoint(s_state.prev_samples[0], s_state.prev_samples[1],
                                        s_state.prev_samples[2], sample);
    s_state.prev_samples[0] = s_state.prev_samples[1];
    s_state.prev_samples[1] = s_state.prev_samples[2];
    s_state.prev_samples[2] = sample;
  }

  return out_pos;
}

//! Read raw PCM data from the stream and convert it. Once the stream is closed
//! and empty, flushes the interpolation delay line.
//! @return Number of output samples written, 0 on underrun or when fully drained
static uint32_t prv_read_and_convert_pcm(int16_t *out, uint32_t max_out_samples) {
  uint32_t bytes_per_sample = prv_pcm_bytes_per_sample();
  uint32_t input_samples = prv_pcm_input_samples(max_out_samples);

  uint32_t bytes_read =
      pcm_stream_read(&s_state.pcm_stream, s_state.raw_buf, input_samples * bytes_per_sample);
  if (bytes_read > 0) {
    if (!(s_state.pcm_format & 1)) {
      s_state.pcm_tail_samples = ARRAY_LENGTH(s_state.prev_samples);
    }
    return prv_convert_pcm(out, s_state.raw_buf, bytes_read / bytes_per_sample);
  }

  if (!pcm_stream_is_done(&s_state.pcm_stream)) {
    return 0;
  }

  uint32_t tail = MIN(s_state.pcm_tail_samples, input_samples);
  s_state.pcm_tail_samples -= tail;
  return prv_convert_pcm(out, nullptr, tail);
}

//! Underrun: run silence through the interpolator so the delayed samples play out.
static uint32_t prv_pcm_silence(int16_t *out, uint32_t max_out_samples) {
  return prv_convert_pcm(out, nullptr, prv_pcm_input_samples(max_out_samples));
}

//! Caller must hold s_lock.
static void prv_refill_locked(void) {
  if (s_state.state == SpeakerStateIdle) {
    return;
  }

  uint32_t samples_generated = 0;
  bool source_exhausted = false;

  if (s_state.source_type == SpeakerSourceNoteSeq) {
    samples_generated =
        note_seq_fill(&s_state.note_seq, s_state.refill_buf, SPEAKER_REFILL_SAMPLES);
    source_exhausted = (samples_generated == 0);
  } else if (s_state.source_type == SpeakerSourceStream) {
    samples_generated = prv_read_and_convert_pcm(s_state.refill_buf, SPEAKER_REFILL_SAMPLES);
    if (samples_generated == 0 && pcm_stream_is_done(&s_state.pcm_stream)) {
      source_exhausted = true;
    } else if (samples_generated == 0) {
      // No data but not done — write silence to keep DMA fed
      PBL_ANALYTICS_ADD(speaker_stream_underrun_count, 1);
      samples_generated = prv_pcm_silence(s_state.refill_buf, SPEAKER_REFILL_SAMPLES);
    }
  } else if (s_state.source_type == SpeakerSourceTone) {
    uint32_t to_gen = s_state.tone_samples_remaining;
    if (to_gen > SPEAKER_REFILL_SAMPLES) {
      to_gen = SPEAKER_REFILL_SAMPLES;
    }
    if (to_gen == 0) {
      source_exhausted = true;
    } else if (s_state.tone_phase_inc == 0) {
      memset(s_state.refill_buf, 0, to_gen * sizeof(int16_t));
    } else {
      for (uint32_t i = 0; i < to_gen; i++) {
        s_state.refill_buf[i] = note_synth_sample(s_state.tone_waveform, s_state.tone_phase_acc,
                                                  s_state.tone_phase_inc, s_state.tone_velocity);
        s_state.tone_phase_acc += s_state.tone_phase_inc;
      }
    }
    s_state.tone_samples_remaining -= to_gen;
    samples_generated = to_gen;
  } else if (s_state.source_type == SpeakerSourceTracks) {
    memset(s_state.mix_buf, 0, sizeof(int32_t) * SPEAKER_REFILL_SAMPLES);
    uint32_t max_generated = 0;
    for (uint32_t i = 0; i < s_state.num_tracks; i++) {
      uint32_t n = track_fill(&s_state.tracks[i], s_state.track_scratch, SPEAKER_REFILL_SAMPLES);
      for (uint32_t j = 0; j < n; j++) {
        s_state.mix_buf[j] += s_state.track_scratch[j];
      }
      if (n > max_generated) {
        max_generated = n;
      }
    }
    if (max_generated == 0) {
      source_exhausted = true;
    }
    for (uint32_t j = 0; j < max_generated; j++) {
      int32_t v = s_state.mix_buf[j];
      if (v > 32767)
        v = 32767;
      else if (v < -32768)
        v = -32768;
      s_state.refill_buf[j] = (int16_t)v;
    }
    samples_generated = max_generated;
  }

  if (source_exhausted) {
    // The queued samples play out before the bottom half ends playback
    s_state.pipeline_draining = true;
    s_state.state = SpeakerStateDraining;
    return;
  }

  if (samples_generated > 0) {
    prv_fifo_write(s_state.refill_buf, samples_generated);
  }
}

static void prv_refill_realtime_locked(void) {
  const uint32_t bytes_needed =
      prv_pcm_input_samples(SPEAKER_REFILL_SAMPLES) * prv_pcm_bytes_per_sample();
  while (s_state.state == SpeakerStatePlaying &&
         pcm_stream_available(&s_state.pcm_stream) >= bytes_needed &&
         prv_fifo_free() >= sizeof(s_state.refill_buf)) {
    prv_refill_locked();
  }
}

bool speaker_service_play_note_seq_owned(const SpeakerNote *notes, uint32_t num_notes,
                                         SpeakerPriority pri, uint8_t vol, PebbleTask owner) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (!s_state.initialized || !notes || num_notes == 0) {
    pbl_mutex_unlock(&s_lock);
    return false;
  }

  if (!prv_can_preempt(pri)) {
    pbl_mutex_unlock(&s_lock);
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
    pbl_mutex_unlock(&s_lock);
    return false;
  }
  memcpy(s_state.note_buf, notes, notes_size);

  note_seq_init(&s_state.note_seq, s_state.note_buf, num_notes, SPEAKER_SAMPLE_RATE);

  s_state.state = SpeakerStatePlaying;
  s_state.source_type = SpeakerSourceNoteSeq;
  s_state.priority = pri;
  s_state.volume = vol;
  prv_set_owner_locked(owner);

  if (!prv_start_or_abort_locked(vol)) {
    pbl_mutex_unlock(&s_lock);
    return false;
  }

  // Prime the queue with initial data
  prv_refill_locked();

  pbl_mutex_unlock(&s_lock);
  return true;
}

bool speaker_service_play_note_seq(const SpeakerNote *notes, uint32_t num_notes,
                                   SpeakerPriority pri, uint8_t vol) {
  return speaker_service_play_note_seq_owned(notes, num_notes, pri, vol, PebbleTask_Unknown);
}

static bool prv_play_tone_internal(uint16_t freq_hz, uint16_t duration_ms, uint8_t waveform,
                                   uint8_t velocity, SpeakerPriority pri, uint8_t vol,
                                   bool volume_absolute, PebbleTask owner) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (!s_state.initialized || duration_ms == 0) {
    pbl_mutex_unlock(&s_lock);
    return false;
  }

  if (!prv_can_preempt(pri)) {
    pbl_mutex_unlock(&s_lock);
    return false;
  }

  if (s_state.state != SpeakerStateIdle) {
    prv_stop_internal(SpeakerFinishReasonPreempted);
  }

  s_state.tone_samples_remaining = ((uint32_t)duration_ms * SPEAKER_SAMPLE_RATE) / 1000;
  s_state.tone_phase_acc = 0;
  // phase_inc = freq_hz * 65536 / sample_rate (16.16 fixed-point per sample)
  s_state.tone_phase_inc = (freq_hz != 0) ? ((uint32_t)freq_hz * 65536u) / SPEAKER_SAMPLE_RATE : 0;
  s_state.tone_waveform = waveform;
  s_state.tone_velocity = velocity;

  s_state.state = SpeakerStatePlaying;
  s_state.source_type = SpeakerSourceTone;
  s_state.priority = pri;
  s_state.volume = vol;
  s_state.volume_absolute = volume_absolute;
  prv_set_owner_locked(owner);

  if (!prv_start_or_abort_locked(vol)) {
    pbl_mutex_unlock(&s_lock);
    return false;
  }
  prv_refill_locked();

  pbl_mutex_unlock(&s_lock);
  return true;
}

bool speaker_service_play_tone(uint16_t freq_hz, uint16_t duration_ms, uint8_t waveform,
                               uint8_t velocity, SpeakerPriority pri, uint8_t vol) {
  return prv_play_tone_internal(freq_hz, duration_ms, waveform, velocity, pri, vol,
                                false /* volume_absolute */, PebbleTask_Unknown);
}

bool speaker_service_play_tone_owned(uint16_t freq_hz, uint16_t duration_ms, uint8_t waveform,
                                     uint8_t velocity, SpeakerPriority pri, uint8_t vol,
                                     PebbleTask owner) {
  return prv_play_tone_internal(freq_hz, duration_ms, waveform, velocity, pri, vol,
                                false /* volume_absolute */, owner);
}

bool speaker_service_play_volume_preview(uint8_t vol, PebbleTask owner) {
  if (vol > 100) {
    vol = 100;
  }
  return prv_play_tone_internal(VOLUME_PREVIEW_FREQ_HZ, VOLUME_PREVIEW_DURATION_MS,
                                SpeakerWaveformSquare, 0 /* velocity: full */, SpeakerPriorityApp,
                                vol, true /* volume_absolute */, owner);
}

bool speaker_service_play_tracks_owned(const SpeakerTrack *tracks, uint32_t num_tracks,
                                       SpeakerPriority pri, uint8_t vol, PebbleTask owner) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (!s_state.initialized || !tracks || num_tracks == 0 || num_tracks > SPEAKER_MAX_TRACKS) {
    pbl_mutex_unlock(&s_lock);
    return false;
  }

  uint32_t total_sample_bytes = 0;
  for (uint32_t i = 0; i < num_tracks; i++) {
    const SpeakerTrack *t = &tracks[i];
    if (!t->notes || t->num_notes == 0) {
      pbl_mutex_unlock(&s_lock);
      return false;
    }
    if (t->sample) {
      if (!t->sample->data || t->sample->num_bytes == 0) {
        pbl_mutex_unlock(&s_lock);
        return false;
      }
      total_sample_bytes += t->sample->num_bytes;
      if (total_sample_bytes > SPEAKER_MAX_SAMPLE_BYTES_TOTAL) {
        pbl_mutex_unlock(&s_lock);
        return false;
      }
    }
  }

  if (!prv_can_preempt(pri)) {
    pbl_mutex_unlock(&s_lock);
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
  prv_set_owner_locked(owner);

  if (!prv_start_or_abort_locked(vol)) {
    pbl_mutex_unlock(&s_lock);
    return false;
  }
  prv_refill_locked();

  pbl_mutex_unlock(&s_lock);
  return true;

alloc_fail:
  for (uint32_t i = 0; i < num_tracks; i++) {
    if (s_state.track_notes_buf[i]) {
      kernel_free(s_state.track_notes_buf[i]);
      s_state.track_notes_buf[i] = nullptr;
    }
    if (s_state.track_sample_data[i]) {
      kernel_free(s_state.track_sample_data[i]);
      s_state.track_sample_data[i] = nullptr;
    }
  }
  pbl_mutex_unlock(&s_lock);
  return false;
}

bool speaker_service_play_tracks(const SpeakerTrack *tracks, uint32_t num_tracks,
                                 SpeakerPriority pri, uint8_t vol) {
  return speaker_service_play_tracks_owned(tracks, num_tracks, pri, vol, PebbleTask_Unknown);
}

bool speaker_service_stream_open(SpeakerPriority pri, uint8_t vol, SpeakerPcmFormat fmt) {
  return speaker_service_stream_open_owned(pri, vol, fmt, PebbleTask_Unknown);
}

static bool prv_stream_open(SpeakerPriority pri, uint8_t vol, SpeakerPcmFormat fmt,
                            PebbleTask owner, bool realtime) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (!s_state.initialized) {
    pbl_mutex_unlock(&s_lock);
    return false;
  }

  if (!prv_can_preempt(pri)) {
    pbl_mutex_unlock(&s_lock);
    return false;
  }

  // Stop any existing playback
  if (s_state.state != SpeakerStateIdle) {
    prv_stop_internal(SpeakerFinishReasonPreempted);
  }

  if (!pcm_stream_init(&s_state.pcm_stream, PCM_STREAM_DEFAULT_SIZE_BYTES)) {
    PBL_LOG_ERR("Failed to allocate PCM stream buffer");
    pbl_mutex_unlock(&s_lock);
    return false;
  }

  s_state.state = SpeakerStatePlaying;
  s_state.source_type = SpeakerSourceStream;
  s_state.priority = pri;
  s_state.volume = vol;
  s_state.pcm_format = fmt;
  s_state.stream_realtime = realtime;
  s_state.owner_task = owner;
  memset(s_state.prev_samples, 0, sizeof(s_state.prev_samples));
  s_state.pcm_tail_samples = 0;

  if (!prv_start_or_abort_locked(vol)) {
    pbl_mutex_unlock(&s_lock);
    return false;
  }

  pbl_mutex_unlock(&s_lock);
  return true;
}

bool speaker_service_stream_open_owned(SpeakerPriority pri, uint8_t vol, SpeakerPcmFormat fmt,
                                       PebbleTask owner) {
  return prv_stream_open(pri, vol, fmt, owner, false);
}

bool speaker_service_stream_open_realtime_owned(SpeakerPriority pri, uint8_t vol,
                                                SpeakerPcmFormat fmt, PebbleTask owner) {
  return prv_stream_open(pri, vol, fmt, owner, true);
}

uint32_t speaker_service_stream_write(const void *data, uint32_t num_bytes) {
  return speaker_service_stream_write_owned(PebbleTask_Unknown, data, num_bytes);
}

uint32_t speaker_service_stream_write_owned(PebbleTask owner, const void *data,
                                            uint32_t num_bytes) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (s_state.state == SpeakerStateIdle || s_state.source_type != SpeakerSourceStream ||
      (owner != PebbleTask_Unknown && owner != s_state.owner_task)) {
    pbl_mutex_unlock(&s_lock);
    return 0;
  }

  num_bytes -= num_bytes % prv_pcm_bytes_per_sample();
  uint32_t written = pcm_stream_write(&s_state.pcm_stream, data, num_bytes);
  if (s_state.stream_realtime) {
    prv_refill_realtime_locked();
  }
  pbl_mutex_unlock(&s_lock);
  return written;
}

void speaker_service_stream_close(void) {
  speaker_service_stream_close_owned(PebbleTask_Unknown);
}

void speaker_service_stream_close_owned(PebbleTask owner) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (s_state.source_type != SpeakerSourceStream ||
      (owner != PebbleTask_Unknown && owner != s_state.owner_task)) {
    pbl_mutex_unlock(&s_lock);
    return;
  }

  s_state.stream_realtime = false;
  // The driver may still have queued audio even when the PCM ring is empty.
  pcm_stream_mark_closing(&s_state.pcm_stream);
  s_state.state = SpeakerStateDraining;

  pbl_mutex_unlock(&s_lock);
}

void speaker_service_stop(void) {
  speaker_service_stop_owned(PebbleTask_Unknown);
}

//! Caller must hold s_lock.
static void prv_stop_owned_locked(PebbleTask owner, bool notify) {
  if (s_state.state == SpeakerStateIdle ||
      (owner != PebbleTask_Unknown && owner != s_state.owner_task)) {
    return;
  }
  if (!notify) {
    s_state.finish_enabled = false;
  }
  prv_stop_internal(SpeakerFinishReasonStopped);
}

void speaker_service_stop_owned(PebbleTask owner) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  prv_stop_owned_locked(owner, true);
  pbl_mutex_unlock(&s_lock);
}

void speaker_service_set_volume(uint8_t vol) {
  speaker_service_set_volume_owned(PebbleTask_Unknown, vol);
}

void speaker_service_set_volume_owned(PebbleTask owner, uint8_t vol) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (owner != PebbleTask_Unknown && owner != s_state.owner_task) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  s_state.volume = vol;
  if (s_state.state != SpeakerStateIdle) {
    const uint8_t effective_vol = prv_effective_volume(vol);
    prv_update_volume_analytics(effective_vol);
    prv_apply_volume(effective_vol);
  }
  pbl_mutex_unlock(&s_lock);
}

bool speaker_service_is_muted(void) {
  return prv_is_speaker_muted(SpeakerPriorityApp);
}

void speaker_service_handle_audio_prefs_changed(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_state.state == SpeakerStateIdle) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  const uint8_t effective_vol = prv_effective_volume(s_state.volume);
  prv_update_volume_analytics(effective_vol);
  prv_apply_volume(effective_vol);
  pbl_mutex_unlock(&s_lock);
}

SpeakerState speaker_service_get_state(void) {
  return s_state.state;
}

void speaker_service_stop_for_task(PebbleTask task) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  // The task is going away, no one to receive the finish event.
  prv_stop_owned_locked(task, false);
  if (s_state.finish_task == task) {
    s_state.finish_enabled = false;
    s_state.finish_task = PebbleTask_Unknown;
  }
  pbl_mutex_unlock(&s_lock);
}

void speaker_service_register_finish(PebbleTask task) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_state.finish_enabled = true;
  s_state.finish_task = task;
  pbl_mutex_unlock(&s_lock);
}

void pbl_analytics_external_collect_speaker_stats(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);

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

  pbl_mutex_unlock(&s_lock);
}

#else // !CONFIG_SPEAKER

void speaker_service_init(void) {
}

bool speaker_service_play_note_seq(const SpeakerNote *notes, uint32_t num_notes,
                                   SpeakerPriority pri, uint8_t vol) {
  return false;
}

bool speaker_service_play_tone(uint16_t freq_hz, uint16_t duration_ms, uint8_t waveform,
                               uint8_t velocity, SpeakerPriority pri, uint8_t vol) {
  return false;
}

bool speaker_service_play_note_seq_owned(const SpeakerNote *notes, uint32_t num_notes,
                                         SpeakerPriority pri, uint8_t vol, PebbleTask owner) {
  return false;
}

bool speaker_service_play_tone_owned(uint16_t freq_hz, uint16_t duration_ms, uint8_t waveform,
                                     uint8_t velocity, SpeakerPriority pri, uint8_t vol,
                                     PebbleTask owner) {
  return false;
}

bool speaker_service_play_tracks_owned(const SpeakerTrack *tracks, uint32_t num_tracks,
                                       SpeakerPriority pri, uint8_t vol, PebbleTask owner) {
  return false;
}

bool speaker_service_play_volume_preview(uint8_t vol, PebbleTask owner) {
  return false;
}

bool speaker_service_play_tracks(const SpeakerTrack *tracks, uint32_t num_tracks,
                                 SpeakerPriority pri, uint8_t vol) {
  return false;
}

bool speaker_service_stream_open(SpeakerPriority pri, uint8_t vol, SpeakerPcmFormat fmt) {
  return false;
}

bool speaker_service_stream_open_owned(SpeakerPriority pri, uint8_t vol, SpeakerPcmFormat fmt,
                                       PebbleTask owner) {
  return false;
}

bool speaker_service_stream_open_realtime_owned(SpeakerPriority pri, uint8_t vol,
                                                SpeakerPcmFormat fmt, PebbleTask owner) {
  return false;
}

uint32_t speaker_service_stream_write_owned(PebbleTask owner, const void *data,
                                            uint32_t num_bytes) {
  return 0;
}

uint32_t speaker_service_stream_write(const void *data, uint32_t num_bytes) {
  return 0;
}

void speaker_service_stream_close(void) {
}

void speaker_service_stream_close_owned(PebbleTask owner) {
}
void speaker_service_stop(void) {
}
void speaker_service_stop_owned(PebbleTask owner) {
}
void speaker_service_set_volume(uint8_t vol) {
}
void speaker_service_set_volume_owned(PebbleTask owner, uint8_t vol) {
}

SpeakerState speaker_service_get_state(void) {
  return SpeakerStateIdle;
}

void speaker_service_stop_for_task(PebbleTask task) {
}
void speaker_service_register_finish(PebbleTask task) {
}
void pbl_analytics_external_collect_speaker_stats(void) {
}

bool speaker_service_is_muted(void) {
  return false;
}
void speaker_service_handle_audio_prefs_changed(void) {
}

#endif // CONFIG_SPEAKER
