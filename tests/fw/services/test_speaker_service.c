/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/speaker/speaker_service.h"

#include <pbl/drivers/audio.h>
#include <pbl/drivers/rtc.h>

#include <string.h>

#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_analytics.h"
#include "stubs_rtc.h"
#include "stubs_do_not_disturb.h"
#include "fake_mutex.h"
#include "fake_pbl_malloc.h"
#include "fake_system_task.h"
#include "fake_events.h"
#include "fake_pebble_tasks.h"

// Alerts preferences: speaker unmuted, no volume cap
bool alerts_preferences_get_speaker_muted(void) {
  return false;
}
uint8_t alerts_preferences_get_speaker_volume(void) {
  return 100;
}
bool alerts_preferences_dnd_get_mute_speaker(void) {
  return false;
}

// ---------------------------------------------------------------------------
// Fake audio driver. Counts what the service hands to the hardware so the
// tests can tell how much audio would actually have been played before the
// service powered the speaker down.

static AudioTransCB s_trans_cb;
static int s_start_count;
static int s_stop_count;
static uint32_t s_samples_written;
static uint32_t s_nonzero_samples;
static int16_t s_output[4096];
static unsigned s_output_count;

void audio_init(AudioDevice *device) {
}

void audio_start(AudioDevice *device, AudioTransCB cb) {
  s_trans_cb = cb;
  s_start_count++;
}

uint32_t audio_write(AudioDevice *device, void *buf, uint32_t size) {
  const int16_t *samples = buf;
  const uint32_t num_samples = size / sizeof(int16_t);
  for (uint32_t i = 0; i < num_samples; i++) {
    if (samples[i] != 0) {
      s_nonzero_samples++;
    }
  }
  for (uint32_t i = 0; i < num_samples && s_output_count < 4096; ++i) {
    s_output[s_output_count++] = samples[i];
  }
  s_samples_written += num_samples;
  return 0;
}

void audio_set_volume(AudioDevice *device, int volume) {
}

void audio_stop(AudioDevice *device) {
  s_stop_count++;
  s_trans_cb = NULL;
}

// ---------------------------------------------------------------------------

#define SAMPLE_RATE 16000
// Must match SPEAKER_PIPELINE_DRAIN_SAMPLES in speaker_service.c
#define DRAIN_SAMPLES ((SAMPLE_RATE * 80) / 1000)

// Simulate the driver requesting more data until playback stops on its own.
static void prv_pump_until_idle(void) {
  for (int i = 0; i < 100 && s_trans_cb; i++) {
    uint32_t free_size = 4096;
    s_trans_cb(&free_size);
    fake_system_task_callbacks_invoke_pending();
  }
}

void test_speaker_service__initialize(void) {
  fake_system_task_callbacks_cleanup();
  s_trans_cb = NULL;
  s_start_count = 0;
  s_stop_count = 0;
  s_samples_written = 0;
  s_nonzero_samples = 0;
  s_output_count = 0;
  speaker_service_init();
}

void test_speaker_service__cleanup(void) {
  speaker_service_stop();
  fake_system_task_callbacks_cleanup();
}

// A tone shorter than the driver's DMA pipeline (~64 ms) must still be
// generated in full and followed by enough padding silence that the queued
// samples play out before the service stops the hardware.
void test_speaker_service__short_tone_drains_pipeline_before_stop(void) {
  const uint16_t duration_ms = 25;
  const uint32_t tone_samples = (SAMPLE_RATE * duration_ms) / 1000;

  cl_assert(speaker_service_play_tone(1000, duration_ms, 0 /* sine */, 0 /* full velocity */,
                                      SpeakerPriorityApp, 80));
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStatePlaying);

  prv_pump_until_idle();

  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateIdle);
  cl_assert_equal_i(s_start_count, 1);
  cl_assert_equal_i(s_stop_count, 1);
  // The full tone reached the driver (1 kHz sine: allow for zero crossings)
  cl_assert(s_nonzero_samples > tone_samples / 2);
  // ...followed by at least a pipeline depth of padding silence
  cl_assert(s_samples_written >= tone_samples + DRAIN_SAMPLES);
}

// While only padding silence remains, a new same-priority sound must be able
// to start instead of being rejected (rapid re-triggers, e.g. a metronome).
void test_speaker_service__same_priority_preempts_during_drain(void) {
  cl_assert(speaker_service_play_tone(1000, 25, 0, 0, SpeakerPriorityApp, 80));

  uint32_t free_size = 4096;
  s_trans_cb(&free_size);
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateDraining);

  cl_assert(speaker_service_play_tone(2000, 25, 0, 0, SpeakerPriorityApp, 80));
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStatePlaying);

  prv_pump_until_idle();
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateIdle);
}

// A sound still generating real samples must NOT be preemptable by the same
// priority (unchanged behavior).
void test_speaker_service__same_priority_cannot_preempt_while_playing(void) {
  cl_assert(speaker_service_play_tone(1000, 500, 0, 0, SpeakerPriorityApp, 80));
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStatePlaying);

  cl_assert(!speaker_service_play_tone(2000, 25, 0, 0, SpeakerPriorityApp, 80));
}

#define RESAMPLE_INPUT_SAMPLES 600

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
    uint32_t space = 4096;
    s_trans_cb(&space);
    fake_system_task_callbacks_invoke_pending();
    offset += count;
  }
  cl_assert_equal_i(s_output_count, 2 * RESAMPLE_INPUT_SAMPLES);
  memcpy(output, s_output, 2 * RESAMPLE_INPUT_SAMPLES * sizeof(int16_t));
  speaker_service_stop();
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
  uint32_t space = 4096;
  s_trans_cb(&space);
  fake_system_task_callbacks_invoke_pending();
  speaker_service_stream_close();
  prv_pump_until_idle();
  const int16_t expected[] = {0, 0, 0, 0, 0, -1000, 0, 9000, 16000, 9000, 0, -1000};
  cl_assert_equal_m(s_output, expected, sizeof(expected));
  cl_assert_equal_i(s_samples_written, sizeof(expected) / sizeof(expected[0]) + DRAIN_SAMPLES);
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStateIdle);
}

// An underrun must play out the delayed samples and filter tail before going
// silent, rather than holding them until the next packet arrives.
void test_speaker_service__upsampling_underrun_runs_silence_through_the_filter(void) {
  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 50, SpeakerPcmFormat_8kHz_16bit));
  const int16_t input[] = {0, 0, 16000};
  cl_assert_equal_i(speaker_service_stream_write(input, sizeof(input)), sizeof(input));
  uint32_t space = 4096;
  s_trans_cb(&space);
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(s_samples_written, 6);

  s_trans_cb(&space);
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(s_samples_written, 6 + 512);
  const int16_t expected[] = {0, 0, 0, 0, 0, -1000, 0, 9000, 16000, 9000, 0, -1000, 0, 0};
  cl_assert_equal_m(s_output, expected, sizeof(expected));
  cl_assert_equal_i(s_nonzero_samples, 5);
  cl_assert_equal_i(speaker_service_get_state(), SpeakerStatePlaying);
}

void test_speaker_service__stream_write_keeps_16bit_samples_aligned(void) {
  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 50, SpeakerPcmFormat_8kHz_16bit));
  const uint8_t bytes[3] = {0};
  cl_assert_equal_i(speaker_service_stream_write(bytes, 3), 2);
  cl_assert_equal_i(speaker_service_stream_write(bytes, 1), 0);
  speaker_service_stop();

  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 50, SpeakerPcmFormat_8kHz_8bit));
  cl_assert_equal_i(speaker_service_stream_write(bytes, 3), 3);
}

void test_speaker_service__refill_catches_up_to_driver_capacity(void) {
  cl_assert(speaker_service_stream_open(SpeakerPriorityApp, 50, SpeakerPcmFormat_16kHz_16bit));
  int16_t input[1536] = {0};
  cl_assert_equal_i(speaker_service_stream_write(input, sizeof(input)), sizeof(input));

  uint32_t space = 1024 * sizeof(int16_t);
  s_trans_cb(&space);
  cl_assert_equal_i(s_samples_written, 1024);

  space = 0;
  s_trans_cb(&space);
  cl_assert_equal_i(s_samples_written, 1024);

  space = 1024 * sizeof(int16_t);
  s_trans_cb(&space);
  cl_assert_equal_i(s_samples_written, 1536);
}
