/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <string.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/battery.h>
#include <pbl/drivers/rtc.h>
#include <pbl/services/speaker/speaker_service.h>

#include <clar.h>
#include <fake_events.h>
#include <fake_mutex.h>
#include <fake_pbl_malloc.h>
#include <fake_pebble_tasks.h>
#include <fake_system_task.h>

// Alerts preferences: speaker unmuted by default, no volume cap
static bool s_speaker_muted;
static bool s_dnd_mute_speaker;
static bool s_dnd_active;

bool alerts_preferences_get_speaker_muted(void) {
  return s_speaker_muted;
}
uint8_t alerts_preferences_get_speaker_volume(void) {
  return 100;
}
bool alerts_preferences_dnd_get_mute_speaker(void) {
  return s_dnd_mute_speaker;
}
bool do_not_disturb_is_active(void) {
  return s_dnd_active;
}

bool system_task_add_callback_from_isr_droppable_raised(SystemTaskEventCallback cb, void *data) {
  return system_task_add_callback(cb, data);
}

void pbl_irq_lock(void) {
}

void pbl_irq_unlock(void) {
}

// ---------------------------------------------------------------------------
// Fake speaker device. The tests play its periods by hand; samples the service did not queue
// keep a marker value, so the tests can tell queued audio from the silence the device plays.

#define SAMPLE_RATE   16000
#define PERIOD_FRAMES 512
#define NOT_QUEUED    INT16_MIN

static const struct pbl_audio s_speaker;
const struct pbl_audio *const SPEAKER = &s_speaker;

static struct pbl_audio_stream s_stream;
static bool s_running;
static int s_start_count;
static int s_stop_count;
static int s_high_load;
static int32_t s_gain;
static bool s_mute;
static uint32_t s_samples_written;
static uint32_t s_nonzero_samples;
static int16_t s_output[4096];
static unsigned s_output_count;

int pbl_audio_get_caps(const struct pbl_audio *audio, struct pbl_audio_caps *caps) {
  *caps = (struct pbl_audio_caps){
    .rates = PBL_AUDIO_RATE_16000,
    .min_channels = 1,
    .max_channels = 1,
    .gain_min_mdb = -36000,
    .gain_max_mdb = 0,
    .gain_step_mdb = 500,
  };
  return 0;
}

int pbl_audio_start(const struct pbl_audio *audio, const struct pbl_audio_stream *stream) {
  cl_assert(!s_running);
  s_stream = *stream;
  s_running = true;
  s_start_count++;
  return 0;
}

int pbl_audio_stop(const struct pbl_audio *audio) {
  s_running = false;
  s_stop_count++;
  return 0;
}

int pbl_audio_period_ready(const struct pbl_audio *audio, void *period) {
  return 0;
}

int pbl_audio_set_gain(const struct pbl_audio *audio, int32_t gain_mdb) {
  s_gain = gain_mdb;
  return 0;
}

int pbl_audio_set_mute(const struct pbl_audio *audio, bool mute) {
  s_mute = mute;
  return 0;
}

size_t pbl_audio_ring_bytes(const struct pbl_audio_stream *stream) {
  return stream->period_frames * stream->channels * sizeof(int16_t) * stream->periods;
}

void *pbl_audio_buf_alloc(size_t size) {
  return calloc(1, size);
}

void pbl_audio_buf_free(void *buf) {
  free(buf);
}

int battery_high_load_request(void) {
  s_high_load++;
  return 0;
}

int battery_high_load_release(void) {
  s_high_load--;
  return 0;
}

// The device finishes a period: the service fills the next one from its queue
static void prv_hw_period(void) {
  int16_t period[PERIOD_FRAMES];
  const struct pbl_audio_pos pos = {0};

  cl_assert(s_running);
  for (unsigned i = 0; i < PERIOD_FRAMES; i++) {
    period[i] = NOT_QUEUED;
  }
  s_stream.period_done(SPEAKER, period, &pos, s_stream.arg);

  for (unsigned i = 0; i < PERIOD_FRAMES && period[i] != NOT_QUEUED; i++) {
    s_samples_written++;
    s_nonzero_samples += period[i] != 0 ? 1 : 0;
    if (s_output_count < 4096) {
      s_output[s_output_count++] = period[i];
    }
  }
}

// A period followed by the KernelBG work it scheduled
static void prv_period(void) {
  prv_hw_period();
  fake_system_task_callbacks_invoke_pending();
}

static void prv_pump_until_idle(void) {
  for (int i = 0; i < 200 && s_running; i++) {
    prv_period();
  }
}

void test_speaker_service__initialize(void) {
  fake_system_task_callbacks_cleanup();
  fake_event_init();
  s_running = false;
  s_start_count = 0;
  s_stop_count = 0;
  s_high_load = 0;
  s_gain = 1;
  s_mute = false;
  s_samples_written = 0;
  s_nonzero_samples = 0;
  s_output_count = 0;
  s_volume = -1;
  s_speaker_muted = false;
  s_dnd_mute_speaker = false;
  s_dnd_active = false;
  speaker_service_init();
}

void test_speaker_service__cleanup(void) {
  speaker_service_stop();
  fake_system_task_callbacks_cleanup();
}

// A tone shorter than the device ring must still be played in full, and the device must play
// it out before the service stops it.
void test_speaker_service__short_tone_drains_pipeline_before_stop(void) {
  const uint16_t duration_ms = 25;
  const uint32_t tone_samples = (SAMPLE_RATE * duration_ms) / 1000;

  cl_assert(speaker_service_play_tone(1000, duration_ms, 0 /* sine */, 0 /* full velocity */,
                                      SpeakerPriorityApp, 80));
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStatePlaying);
  cl_assert_equal_i(s_high_load, 1);

  // The tone is queued; the device must hand out a full ring of silence after it
  prv_period();
  cl_assert_equal_i(s_samples_written, tone_samples);
  prv_period();
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateDraining);
  cl_assert_equal_i(s_stop_count, 0);

  prv_pump_until_idle();

  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateIdle);
  cl_assert_equal_i(s_start_count, 1);
  cl_assert_equal_i(s_stop_count, 1);
  cl_assert_equal_i(s_high_load, 0);
  // The full tone reached the device (1 kHz sine: allow for zero crossings)
  cl_assert(s_nonzero_samples > tone_samples / 2);
  cl_assert_equal_i(s_samples_written, tone_samples);
}

// While only the queued tail remains, a new same-priority sound must be able to start instead
// of being rejected (rapid re-triggers, e.g. a metronome).
void test_speaker_service__same_priority_preempts_during_drain(void) {
  cl_assert(speaker_service_play_tone(1000, 25, 0, 0, SpeakerPriorityApp, 80));

  prv_period();
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateDraining);

  cl_assert(speaker_service_play_tone(2000, 25, 0, 0, SpeakerPriorityApp, 80));
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStatePlaying);

  prv_pump_until_idle();
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateIdle);
  cl_assert_equal_i(s_start_count, 2);
  cl_assert_equal_i(s_stop_count, 2);
}

// A sound still generating real samples must NOT be preemptable by the same priority.
void test_speaker_service__same_priority_cannot_preempt_while_playing(void) {
  cl_assert(speaker_service_play_tone(1000, 500, 0, 0, SpeakerPriorityApp, 80));
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStatePlaying);

  cl_assert(!speaker_service_play_tone(2000, 25, 0, 0, SpeakerPriorityApp, 80));
}

void test_speaker_service__owned_stream_cannot_write_or_stop_a_preempting_stream(void) {
  const int16_t samples[] = {100, -100};
  cl_assert(speaker_service_stream_open_owned(SpeakerPriorityNotification, 30,
                                              SpeakerPcmFormat_8kHz_16bit, PebbleTask_BTHCI));
  cl_assert_equal_i(speaker_service_stream_write_owned(PebbleTask_BTHCI, samples, sizeof(samples)),
                    sizeof(samples));
  cl_assert(speaker_service_stream_open_owned(SpeakerPriorityCritical, 30,
                                              SpeakerPcmFormat_8kHz_16bit, PebbleTask_KernelMain));
  cl_assert_equal_i(speaker_service_stream_write_owned(PebbleTask_BTHCI, samples, sizeof(samples)),
                    0);
  speaker_service_stop_for_task(PebbleTask_BTHCI);
  speaker_service_stream_close_owned(PebbleTask_BTHCI);
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStatePlaying);
  cl_assert_equal_i(
      speaker_service_stream_write_owned(PebbleTask_KernelMain, samples, sizeof(samples)),
      sizeof(samples));
}

void test_speaker_service__stream_refills_from_the_period_wakeup(void) {
  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 30, SpeakerPcmFormat_16kHz_16bit));
  int16_t samples[512];
  for (unsigned i = 0; i < 512; ++i) {
    samples[i] = 100;
  }
  cl_assert_equal_i(speaker_service_stream_write(samples, sizeof(samples)), sizeof(samples));
  prv_hw_period();
  cl_assert_equal_i(s_samples_written, 0);
  cl_assert_equal_i(fake_system_task_count_callbacks(), 1);

  // A wakeup already scheduled is not scheduled again
  prv_hw_period();
  cl_assert_equal_i(fake_system_task_count_callbacks(), 1);

  fake_system_task_callbacks_invoke_pending();
  prv_hw_period();
  cl_assert_equal_i(s_samples_written, 512);
  cl_assert_equal_i(s_nonzero_samples, 512);
}

void test_speaker_service__live_stream_waits_for_a_whole_refill(void) {
  cl_assert(speaker_service_stream_open_realtime_owned(
      SpeakerPriorityNotification, 50, SpeakerPcmFormat_8kHz_16bit, PebbleTask_BTHCI));
  int16_t samples[256] = {100};
  prv_period();
  cl_assert_equal_i(s_samples_written, 0);
  cl_assert_equal_i(speaker_service_stream_write_owned(PebbleTask_BTHCI, samples, 480), 480);
  prv_period();
  cl_assert_equal_i(s_samples_written, 0);
  cl_assert_equal_i(speaker_service_stream_write_owned(PebbleTask_BTHCI, samples, 32), 32);
  prv_hw_period();
  cl_assert_equal_i(s_samples_written, 512);
  prv_period();
  cl_assert_equal_i(s_samples_written, 512);
}

void test_speaker_service__live_stream_close_drains_a_partial_refill(void) {
  cl_assert(speaker_service_stream_open_realtime_owned(
      SpeakerPriorityNotification, 50, SpeakerPcmFormat_8kHz_16bit, PebbleTask_BTHCI));
  int16_t samples[20] = {100};
  cl_assert_equal_i(speaker_service_stream_write_owned(PebbleTask_BTHCI, samples, sizeof(samples)),
                    sizeof(samples));
  speaker_service_stream_close_owned(PebbleTask_BTHCI);
  prv_pump_until_idle();
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateIdle);
  cl_assert_equal_i(s_samples_written, 40 + 6);
}

void test_speaker_service__live_stream_close_preserves_already_queued_audio(void) {
  cl_assert(speaker_service_stream_open_realtime_owned(
      SpeakerPriorityNotification, 50, SpeakerPcmFormat_8kHz_16bit, PebbleTask_BTHCI));
  int16_t samples[256] = {100};
  speaker_service_stream_write_owned(PebbleTask_BTHCI, samples, sizeof(samples));
  speaker_service_stream_close_owned(PebbleTask_BTHCI);
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateDraining);
  prv_pump_until_idle();
  cl_assert_equal_i(s_samples_written, 512 + 6);
}

#define RESAMPLE_INPUT_SAMPLES 600

static void prv_close_after_pcm_ring_is_empty(SpeakerPcmFormat format) {
  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 50, format));
  speaker_service_register_finish(PebbleTask_App);
  const uint8_t samples[1024] = {100};
  cl_assert_equal_i(speaker_service_stream_write(samples, sizeof(samples)), sizeof(samples));
  prv_period();
  prv_hw_period();
  prv_hw_period();
  const unsigned generated = s_samples_written;
  cl_assert_equal_i(generated, format == SpeakerPcmFormat_16kHz_8bit ? 1024 : 512);

  speaker_service_stream_close();
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateDraining);
  cl_assert_equal_i(s_stop_count, 0);
  cl_assert_equal_i(fake_event_get_count(), 0);
  prv_pump_until_idle();
  cl_assert_equal_i(s_samples_written, generated);
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateIdle);
  cl_assert_equal_i(s_stop_count, 1);
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert_equal_i(fake_event_get_last().speaker.finish_reason, SpeakerFinishReasonDone);
}

void test_speaker_service__close_empty_pcm_ring_preserves_16khz_16bit_pipeline(void) {
  prv_close_after_pcm_ring_is_empty(SpeakerPcmFormat_16kHz_16bit);
}

void test_speaker_service__close_empty_pcm_ring_preserves_16khz_8bit_pipeline(void) {
  prv_close_after_pcm_ring_is_empty(SpeakerPcmFormat_16kHz_8bit);
}

static void prv_repeated_close_during_drain(bool realtime, SpeakerPcmFormat format) {
  speaker_service_register_finish(PebbleTask_BTHCI);
  if (realtime) {
    cl_assert(speaker_service_stream_open_realtime_owned(SpeakerPriorityNotification, 50, format,
                                                         PebbleTask_BTHCI));
  } else {
    cl_assert(speaker_service_stream_open_owned(SpeakerPriorityNotification, 50, format,
                                                PebbleTask_BTHCI));
  }
  const int16_t samples[] = {100, -100};
  cl_assert_equal_i(speaker_service_stream_write_owned(PebbleTask_BTHCI, samples, sizeof(samples)),
                    sizeof(samples));
  speaker_service_stream_close_owned(PebbleTask_BTHCI);
  prv_period();
  prv_period();

  speaker_service_stream_close_owned(PebbleTask_BTHCI);
  speaker_service_stream_close_owned(PebbleTask_KernelMain);
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateDraining);
  cl_assert_equal_i(s_stop_count, 0);
  cl_assert_equal_i(fake_event_get_count(), 0);
  prv_pump_until_idle();
  const unsigned generated = format == SpeakerPcmFormat_8kHz_16bit ? 10 : 2;
  cl_assert_equal_i(s_samples_written, generated);
  cl_assert_equal_i(s_stop_count, 1);
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert_equal_i(fake_event_get_last().speaker.finish_reason, SpeakerFinishReasonDone);
  speaker_service_stream_close_owned(PebbleTask_BTHCI);
  cl_assert_equal_i(fake_event_get_count(), 1);
}

void test_speaker_service__repeated_close_preserves_normal_16khz_drain(void) {
  prv_repeated_close_during_drain(false, SpeakerPcmFormat_16kHz_16bit);
}

void test_speaker_service__repeated_close_preserves_normal_8khz_drain(void) {
  prv_repeated_close_during_drain(false, SpeakerPcmFormat_8kHz_16bit);
}

void test_speaker_service__repeated_close_preserves_realtime_16khz_drain(void) {
  prv_repeated_close_during_drain(true, SpeakerPcmFormat_16kHz_16bit);
}

void test_speaker_service__repeated_close_preserves_realtime_8khz_drain(void) {
  prv_repeated_close_during_drain(true, SpeakerPcmFormat_8kHz_16bit);
}

void test_speaker_service__stop_still_cancels_a_draining_stream_immediately(void) {
  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 50, SpeakerPcmFormat_16kHz_16bit));
  speaker_service_register_finish(PebbleTask_App);
  const int16_t samples[] = {100, -100};
  cl_assert_equal_i(speaker_service_stream_write(samples, sizeof(samples)), sizeof(samples));
  speaker_service_stream_close();
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateDraining);

  speaker_service_stop();
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateIdle);
  cl_assert_equal_i(s_stop_count, 1);
  cl_assert_equal_i(s_samples_written, 0);
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert_equal_i(fake_event_get_last().speaker.finish_reason, SpeakerFinishReasonStopped);
  speaker_service_stream_close();
  cl_assert_equal_i(s_stop_count, 1);
}

void test_speaker_service__closing_an_empty_stream_finishes_asynchronously(void) {
  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 50, SpeakerPcmFormat_16kHz_16bit));
  speaker_service_stream_close();
  speaker_service_stream_close();
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateDraining);
  cl_assert_equal_i(s_stop_count, 0);
  const int16_t samples[] = {100, -100};
  cl_assert_equal_i(speaker_service_stream_write(samples, sizeof(samples)), 0);
  prv_pump_until_idle();
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateIdle);
  cl_assert_equal_i(s_samples_written, 0);
  cl_assert_equal_i(s_stop_count, 1);
}

static int16_t prv_cubic_midpoint(int16_t s0, int16_t s1, int16_t s2, int16_t s3) {
  int32_t v = -(int32_t)s0 + 9 * (int32_t)s1 + 9 * (int32_t)s2 - (int32_t)s3;
  return (int16_t)((v + 8) >> 4);
}

// Reference 2x upsampler: two input samples of delay, then each input sample
// followed by the cubic midpoint towards the next one.
static void prv_reference_upsample(const int16_t *input, unsigned count, int16_t *output) {
  for (unsigned i = 0; i < count; ++i) {
    int16_t taps[4];
    for (unsigned k = 0; k < 4; ++k) {
      taps[k] = (i + k >= 3) ? input[i + k - 3] : 0;
    }
    output[2 * i] = taps[1];
    output[2 * i + 1] = prv_cubic_midpoint(taps[0], taps[1], taps[2], taps[3]);
  }
}

static void prv_resample_partition(const int16_t *input, unsigned partition, int16_t *output) {
  s_output_count = 0;
  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 50, SpeakerPcmFormat_8kHz_16bit));
  for (unsigned offset = 0; offset < RESAMPLE_INPUT_SAMPLES;) {
    unsigned count =
        partition < RESAMPLE_INPUT_SAMPLES - offset ? partition : RESAMPLE_INPUT_SAMPLES - offset;
    cl_assert_equal_i(speaker_service_stream_write(input + offset, count * 2), count * 2);
    prv_period();
    offset += count;
  }
  prv_hw_period();
  prv_hw_period();
  prv_hw_period();
  cl_assert(s_output_count >= 2 * RESAMPLE_INPUT_SAMPLES);
  memcpy(output, s_output, 2 * RESAMPLE_INPUT_SAMPLES * sizeof(int16_t));
  speaker_service_stop();
  fake_system_task_callbacks_cleanup();
}

void test_speaker_service__upsampling_does_not_depend_on_packet_boundaries(void) {
  int16_t input[RESAMPLE_INPUT_SAMPLES];
  int16_t expected[2 * RESAMPLE_INPUT_SAMPLES], actual[2 * RESAMPLE_INPUT_SAMPLES];
  for (unsigned i = 0; i < RESAMPLE_INPUT_SAMPLES; ++i) {
    input[i] = (i * 977 % 24000) - 12000;
  }
  prv_reference_upsample(input, RESAMPLE_INPUT_SAMPLES, expected);

  const unsigned partitions[] = {1, 7, 30, 127, 256};
  for (unsigned i = 0; i < sizeof(partitions) / sizeof(partitions[0]); ++i) {
    prv_resample_partition(input, partitions[i], actual);
    cl_assert_equal_m(actual, expected, sizeof(expected));
  }
}

void test_speaker_service__upsampling_close_flushes_the_last_sample_and_filter_tail(void) {
  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 50, SpeakerPcmFormat_8kHz_16bit));
  const int16_t input[] = {0, 0, 16000};
  cl_assert_equal_i(speaker_service_stream_write(input, sizeof(input)), sizeof(input));
  prv_period();
  speaker_service_stream_close();
  prv_pump_until_idle();
  const int16_t expected[] = {0, 0, 0, 0, 0, -1000, 0, 9000, 16000, 9000, 0, -1000};
  cl_assert_equal_m(s_output, expected, sizeof(expected));
  cl_assert_equal_i(s_samples_written, sizeof(expected) / sizeof(expected[0]));
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateIdle);
}

// An underrun must play out the delayed samples and filter tail before going silent, rather
// than holding them until the next packet arrives.
void test_speaker_service__upsampling_underrun_runs_silence_through_the_filter(void) {
  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 50, SpeakerPcmFormat_8kHz_16bit));
  const int16_t input[] = {0, 0, 16000};
  cl_assert_equal_i(speaker_service_stream_write(input, sizeof(input)), sizeof(input));
  prv_period();
  prv_hw_period();
  cl_assert_equal_i(s_samples_written, 6);

  fake_system_task_callbacks_invoke_pending();
  prv_hw_period();
  cl_assert_equal_i(s_samples_written, 6 + 512);
  const int16_t expected[] = {0, 0, 0, 0, 0, -1000, 0, 9000, 16000, 9000, 0, -1000, 0, 0};
  cl_assert_equal_m(s_output, expected, sizeof(expected));
  cl_assert_equal_i(s_nonzero_samples, 5);
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStatePlaying);
}

void test_speaker_service__stream_write_keeps_16bit_samples_aligned(void) {
  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 50, SpeakerPcmFormat_8kHz_16bit));
  const uint8_t bytes[3] = {};
  cl_assert_equal_i(speaker_service_stream_write(bytes, 3), 2);
  cl_assert_equal_i(speaker_service_stream_write(bytes, 1), 0);
  speaker_service_stop();

  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 50, SpeakerPcmFormat_8kHz_8bit));
  cl_assert_equal_i(speaker_service_stream_write(bytes, 3), 3);
}

// Volume 1-100 maps onto the gain steps between the floor and 0 dB; 0 mutes
void test_speaker_service__volume_maps_to_gain(void) {
  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 100, SpeakerPcmFormat_8kHz_16bit));
  cl_assert_equal_i(s_gain, 0);
  cl_assert(!s_mute);
  speaker_service_set_volume(1);
  cl_assert_equal_i(s_gain, -35500);
  speaker_service_set_volume(30);
  cl_assert_equal_i(s_gain, -25000);
  speaker_service_set_volume(0);
  cl_assert(s_mute);
}

void test_speaker_service__volume_change_cannot_affect_a_preempting_stream(void) {
  cl_assert(speaker_service_stream_open_owned(SpeakerPriorityNotification, 100,
                                              SpeakerPcmFormat_8kHz_16bit, PebbleTask_BTHCI));
  speaker_service_set_volume_owned(PebbleTask_BTHCI, 40);
  cl_assert_equal_i(s_gain, -21500);
  cl_assert(speaker_service_stream_open_owned(SpeakerPriorityCritical, 80,
                                              SpeakerPcmFormat_8kHz_16bit, PebbleTask_KernelMain));
  speaker_service_set_volume_owned(PebbleTask_BTHCI, 20);
  cl_assert_equal_i(s_gain, -7000);
}

void test_speaker_service__refill_catches_up_to_queue_capacity(void) {
  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 50, SpeakerPcmFormat_16kHz_16bit));
  int16_t input[1536] = {};
  cl_assert_equal_i(speaker_service_stream_write(input, sizeof(input)), sizeof(input));

  prv_period();
  for (int i = 0; i < 3; i++) {
    prv_hw_period();
  }
  cl_assert_equal_i(s_samples_written, 1536);
}

void test_speaker_service__quiet_time_mutes_non_critical_sounds(void) {
  s_dnd_mute_speaker = true;
  s_dnd_active = true;

  cl_assert(speaker_service_play_tone(1000, 500, 0, 0, SpeakerPriorityNotification, 80));
  cl_assert_equal_i(s_volume, 0);
  cl_assert(speaker_service_is_muted());
}

void test_speaker_service__quiet_time_does_not_mute_alarms(void) {
  s_dnd_mute_speaker = true;
  s_dnd_active = true;

  cl_assert(speaker_service_play_tone(1000, 500, 0, 0, SpeakerPriorityCritical, 80));
  cl_assert_equal_i(s_volume, 80);

  speaker_service_handle_audio_prefs_changed();
  cl_assert_equal_i(s_volume, 80);
}

void test_speaker_service__speaker_mute_still_mutes_alarms(void) {
  s_speaker_muted = true;

  cl_assert(speaker_service_play_tone(1000, 500, 0, 0, SpeakerPriorityCritical, 80));
  cl_assert_equal_i(s_volume, 0);
}
