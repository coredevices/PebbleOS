/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/audio_encoder/audio_encoder.h"
#include "pbl/services/audio_encoder/audio_encoder_backend.h"

#include <string.h>

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_mutex.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_logging.h"
#include "stubs_passert.h"

// Fake backend
////////////////////////////////////////////////////////////////

static int s_open_count;
static int s_close_count;
static int s_encode_count;
static bool s_open_should_fail;
static int16_t s_last_first_sample;

#define FAKE_FRAME_SAMPLES (160)

static bool prv_open(AudioEncoderInfo *info_out) {
  if (s_open_should_fail) {
    return false;
  }
  s_open_count++;
  *info_out = (AudioEncoderInfo){
    .channels = 1,
    .frame_samples = FAKE_FRAME_SAMPLES,
    .sample_rate = 16000,
    .bitrate = 12000,
    .max_packet_bytes = 40,
    .bitstream_version = 1,
  };
  return true;
}

static int prv_encode(const int16_t *pcm, uint8_t *out, uint32_t out_len) {
  s_encode_count++;
  s_last_first_sample = pcm[0];
  memset(out, 0xAB, out_len < 30 ? out_len : 30);
  return 30;
}

static void prv_close(void) {
  s_close_count++;
}

static const AudioEncoderBackend s_fake_backend = {
  .codec = AudioCodecSpeexWB,
  .open = prv_open,
  .encode = prv_encode,
  .close = prv_close,
};

static const AudioEncoderBackend *const s_backends[] = {&s_fake_backend};

const AudioEncoderBackend *const *audio_encoder_get_backends(size_t *num_backends_out) {
  *num_backends_out = 1;
  return s_backends;
}

// Setup
////////////////////////////////////////////////////////////////

void test_audio_encoder__initialize(void) {
  fake_mutex_reset(false);
  s_open_count = 0;
  s_close_count = 0;
  s_encode_count = 0;
  s_open_should_fail = false;
  audio_encoder_service_init();
}

void test_audio_encoder__cleanup(void) {
  fake_mutex_assert_all_unlocked();
}

// Tests
////////////////////////////////////////////////////////////////

void test_audio_encoder__codec_availability(void) {
  cl_assert(audio_encoder_service_is_codec_available(AudioCodecSpeexWB));
  cl_assert(!audio_encoder_service_is_codec_available(AudioCodecInvalid));
  cl_assert(!audio_encoder_service_is_codec_available((AudioCodec)7));
  cl_assert(!audio_encoder_service_is_codec_available(AudioCodecCount));
}

void test_audio_encoder__open_encode_close(void) {
  AudioEncoderInfo info;
  cl_assert(!audio_encoder_service_open((AudioCodec)7, PebbleTask_App, &info));
  cl_assert(!audio_encoder_service_open(AudioCodecSpeexWB, PebbleTask_App, NULL));
  cl_assert(!audio_encoder_service_is_open());

  cl_assert(audio_encoder_service_open(AudioCodecSpeexWB, PebbleTask_App, &info));
  cl_assert(audio_encoder_service_is_open());
  cl_assert_equal_i(1, s_open_count);
  cl_assert_equal_i(info.codec, AudioCodecSpeexWB);
  cl_assert_equal_i(info.frame_samples, FAKE_FRAME_SAMPLES);
  cl_assert_equal_i(info.sample_rate, 16000);

  int16_t pcm[FAKE_FRAME_SAMPLES] = {[0] = 1234};
  uint8_t out[64];
  // Wrong owner
  cl_assert(audio_encoder_service_encode(PebbleTask_Worker, pcm, FAKE_FRAME_SAMPLES, out,
                                         sizeof(out)) < 0);
  // Wrong frame size
  cl_assert(audio_encoder_service_encode(PebbleTask_App, pcm, FAKE_FRAME_SAMPLES - 1, out,
                                         sizeof(out)) < 0);
  cl_assert(
      audio_encoder_service_encode(PebbleTask_App, NULL, FAKE_FRAME_SAMPLES, out, sizeof(out)) < 0);
  cl_assert_equal_i(0, s_encode_count);

  cl_assert_equal_i(
      30, audio_encoder_service_encode(PebbleTask_App, pcm, FAKE_FRAME_SAMPLES, out, sizeof(out)));
  cl_assert_equal_i(1, s_encode_count);
  cl_assert_equal_i(1234, s_last_first_sample);
  cl_assert_equal_i(0xAB, out[0]);

  // Close by non-owner is ignored
  audio_encoder_service_close(PebbleTask_Worker);
  cl_assert(audio_encoder_service_is_open());
  audio_encoder_service_close(PebbleTask_App);
  cl_assert(!audio_encoder_service_is_open());
  cl_assert_equal_i(1, s_close_count);
  cl_assert(
      audio_encoder_service_encode(PebbleTask_App, pcm, FAKE_FRAME_SAMPLES, out, sizeof(out)) < 0);

  // Double close is harmless
  audio_encoder_service_close(PebbleTask_App);
  cl_assert_equal_i(1, s_close_count);
}

void test_audio_encoder__backend_open_failure(void) {
  AudioEncoderInfo info;
  s_open_should_fail = true;
  cl_assert(!audio_encoder_service_open(AudioCodecSpeexWB, PebbleTask_App, &info));
  cl_assert(!audio_encoder_service_is_open());
}

void test_audio_encoder__system_preempts_app_but_not_vice_versa(void) {
  AudioEncoderInfo info;
  cl_assert(audio_encoder_service_open(AudioCodecSpeexWB, PebbleTask_App, &info));

  // Another app-level open is refused
  cl_assert(!audio_encoder_service_open(AudioCodecSpeexWB, PebbleTask_Worker, &info));
  cl_assert_equal_i(1, s_open_count);

  // The system takes it over
  cl_assert(audio_encoder_service_open(AudioCodecSpeexWB, AUDIO_ENCODER_SYSTEM_OWNER, &info));
  cl_assert_equal_i(2, s_open_count);
  cl_assert_equal_i(1, s_close_count);

  int16_t pcm[FAKE_FRAME_SAMPLES] = {};
  uint8_t out[64];
  cl_assert(
      audio_encoder_service_encode(PebbleTask_App, pcm, FAKE_FRAME_SAMPLES, out, sizeof(out)) < 0);
  cl_assert_equal_i(30, audio_encoder_service_encode(AUDIO_ENCODER_SYSTEM_OWNER, pcm,
                                                     FAKE_FRAME_SAMPLES, out, sizeof(out)));

  // The app cannot take it back while the system holds it
  cl_assert(!audio_encoder_service_open(AudioCodecSpeexWB, PebbleTask_App, &info));

  audio_encoder_service_close_for_task(PebbleTask_App); // no-op
  cl_assert(audio_encoder_service_is_open());
  audio_encoder_service_close(AUDIO_ENCODER_SYSTEM_OWNER);
  cl_assert(!audio_encoder_service_is_open());
  cl_assert_equal_i(2, s_close_count);
}
