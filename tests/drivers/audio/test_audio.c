/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/drivers/audio.h>
#include <pbl/kernel/time.h>

#include <clar.h>

#define PERIOD_FRAMES 8U
#define PERIODS       3U
#define RATE          16000U

static int s_lock_depth;
static uint64_t s_ticks;
static char s_log[64];
static int s_start_ret;
static int s_codec_enable_ret;
static int32_t s_hw_gain;
static bool s_hw_mute;
static int32_t s_codec_gain;
static bool s_codec_mute;
static int s_syncs;
static size_t s_pointer;
static uint8_t s_dai_dir;
static uint8_t s_codec_dir;
static uint8_t s_codec_streaming;
static int32_t s_codec_gains[PBL_AUDIO_DIRS];

static void *s_ring;
static void *s_periods[8];
static struct pbl_audio_pos s_pos[8];
static uint8_t s_dirs[8];
static int s_calls;
static bool s_refill;

void pbl_irq_lock(void) {
  s_lock_depth++;
}

void pbl_irq_unlock(void) {
  s_lock_depth--;
}

void pbl_mutex_init(struct pbl_mutex *mutex) {
}

int pbl_mutex_lock_lr(struct pbl_mutex *mutex, pbl_timeout_t timeout, uintptr_t lr) {
  return 0;
}

void pbl_mutex_unlock(struct pbl_mutex *mutex) {
}

uint64_t pbl_uptime_ticks(void) {
  return s_ticks;
}

void *pbl_malloc(size_t bytes) {
  return malloc(bytes);
}

void pbl_free(void *ptr) {
  free(ptr);
}

static void prv_log(char c) {
  size_t len = strlen(s_log);

  if (len + 1U < sizeof(s_log)) {
    s_log[len] = c;
  }
}

static void prv_get_caps(const struct pbl_audio_dai *dai, enum pbl_audio_dir dir,
                         struct pbl_audio_caps *caps) {
  caps->rates = PBL_AUDIO_RATE_16000;
  caps->min_channels = 1U;
  caps->max_channels = 2U;
  caps->max_periods = 4U;
  caps->gain_min_mdb = -36000;
  caps->gain_max_mdb = 0;
  caps->gain_step_mdb = 500;
  caps->gain_default_mdb = -6000;
}

static int prv_start(const struct pbl_audio_dai *dai, enum pbl_audio_dir dir,
                     const struct pbl_audio_stream *stream, int32_t gain_mdb, bool mute) {
  prv_log('S');
  s_dai_dir = dir;
  s_hw_gain = gain_mdb;
  s_hw_mute = mute;
  return s_start_ret;
}

static int prv_stop(const struct pbl_audio_dai *dai, enum pbl_audio_dir dir) {
  prv_log('s');
  s_dai_dir = dir;
  return 0;
}

static int prv_set_gain(const struct pbl_audio_dai *dai, enum pbl_audio_dir dir, int32_t gain_mdb) {
  s_hw_gain = gain_mdb;
  return 0;
}

static void prv_sync(const struct pbl_audio_dai *dai, void *buf, size_t len) {
  s_syncs++;
}

static int prv_pointer(const struct pbl_audio_dai *dai, enum pbl_audio_dir dir, size_t *frames) {
  *frames = s_pointer;
  return 0;
}

static const struct pbl_audio_dai_ops s_ops = {
  .get_caps = prv_get_caps,
  .start = prv_start,
  .stop = prv_stop,
  .set_gain = prv_set_gain,
  .sync_for_device = prv_sync,
  .sync_for_cpu = prv_sync,
  .pointer = prv_pointer,
};

static void prv_playback_get_caps(const struct pbl_audio_dai *dai, enum pbl_audio_dir dir,
                                  struct pbl_audio_caps *caps) {
  if (dir == PBL_AUDIO_PLAYBACK) {
    prv_get_caps(dai, dir, caps);
  }
}

static const struct pbl_audio_dai_ops s_playback_ops = {
  .get_caps = prv_playback_get_caps,
  .start = prv_start,
  .stop = prv_stop,
};

static void prv_codec_get_caps(const struct pbl_audio_codec *codec, enum pbl_audio_dir dir,
                               struct pbl_audio_caps *caps) {
  caps->gain_min_mdb = -78000;
  caps->gain_max_mdb = 12000;
  caps->gain_step_mdb = 750;
  caps->gain_default_mdb = 0;
}

static int prv_codec_enable(const struct pbl_audio_codec *codec,
                            const struct pbl_audio_codec_cfg *cfg) {
  prv_log('C');
  cl_assert_equal_i(cfg->rate, RATE);
  cl_assert_equal_i(cfg->fmt, PBL_AUDIO_FMT_I2S | PBL_AUDIO_FMT_CODEC_MASTER);
  s_codec_dir = cfg->dir;
  s_codec_streaming = cfg->streaming;
  s_codec_gain = cfg->gain_mdb;
  s_codec_gains[cfg->dir] = cfg->gain_mdb;
  s_codec_mute = cfg->mute;
  return s_codec_enable_ret;
}

static int prv_codec_disable(const struct pbl_audio_codec *codec, enum pbl_audio_dir dir,
                             uint8_t streaming) {
  prv_log('c');
  s_codec_dir = dir;
  s_codec_streaming = streaming;
  return 0;
}

static int prv_codec_set_gain(const struct pbl_audio_codec *codec, enum pbl_audio_dir dir,
                              int32_t gain_mdb) {
  s_codec_gain = gain_mdb;
  s_codec_gains[dir] = gain_mdb;
  return 0;
}

static int prv_codec_set_mute(const struct pbl_audio_codec *codec, enum pbl_audio_dir dir,
                              bool mute) {
  s_codec_mute = mute;
  return 0;
}

static const struct pbl_audio_codec_ops s_codec_ops = {
  .get_caps = prv_codec_get_caps,
  .enable = prv_codec_enable,
  .disable = prv_codec_disable,
  .set_gain = prv_codec_set_gain,
  .set_mute = prv_codec_set_mute,
};

static int prv_amp_enable(const struct pbl_audio_amp *amp) {
  prv_log('A');
  return 0;
}

static int prv_amp_disable(const struct pbl_audio_amp *amp) {
  prv_log('a');
  return 0;
}

static const struct pbl_audio_amp_ops s_amp_ops = {
  .enable = prv_amp_enable,
  .disable = prv_amp_disable,
};

PBL_DEVICE_STATE_DEFINE(s_codec);
static const struct pbl_audio_codec s_codec = {
  .dev = PBL_DEVICE_INIT(s_codec, "codec", nullptr, nullptr),
  .ops = &s_codec_ops,
};

static int prv_dmic_prepare(const struct pbl_audio_codec *codec, enum pbl_audio_dir dir) {
  prv_log('P');
  return 0;
}

static int prv_dmic_unprepare(const struct pbl_audio_codec *codec, enum pbl_audio_dir dir) {
  prv_log('p');
  return 0;
}

static const struct pbl_audio_codec_ops s_dmic_ops = {
  .prepare = prv_dmic_prepare,
  .unprepare = prv_dmic_unprepare,
};

PBL_DEVICE_STATE_DEFINE(s_dmic);
static const struct pbl_audio_codec s_dmic = {
  .dev = PBL_DEVICE_INIT(s_dmic, "dmic", nullptr, nullptr),
  .ops = &s_dmic_ops,
};

PBL_DEVICE_STATE_DEFINE(s_amp);
static const struct pbl_audio_amp s_amp = {
  .dev = PBL_DEVICE_INIT(s_amp, "amp", nullptr, nullptr),
  .ops = &s_amp_ops,
};

PBL_AUDIO_DAI_STATE_DEFINE(s_out_dai);
static const struct pbl_audio_dai s_out_dai =
    PBL_AUDIO_DAI_INIT(s_out_dai, "out_dai", nullptr, &s_ops);
PBL_AUDIO_CARD_DEFINE(s_out, "out", &s_out_dai, nullptr, nullptr, 0, 0);

PBL_AUDIO_DAI_STATE_DEFINE(s_in_dai);
static const struct pbl_audio_dai s_in_dai =
    PBL_AUDIO_DAI_INIT(s_in_dai, "in_dai", nullptr, &s_ops);
PBL_AUDIO_CARD_DEFINE(s_in, "in", &s_in_dai, nullptr, nullptr, 0, 0);

PBL_AUDIO_DAI_STATE_DEFINE(s_linked_dai);
static const struct pbl_audio_dai s_linked_dai =
    PBL_AUDIO_DAI_INIT(s_linked_dai, "linked_dai", nullptr, &s_ops);
PBL_AUDIO_CARD_DEFINE(s_linked, "linked", &s_linked_dai, &s_codec, &s_amp, PBL_AUDIO_FMT_I2S,
                      PBL_AUDIO_FMT_CODEC_MASTER);

PBL_AUDIO_DAI_STATE_DEFINE(s_mic_dai);
static const struct pbl_audio_dai s_mic_dai =
    PBL_AUDIO_DAI_INIT(s_mic_dai, "mic_dai", nullptr, &s_ops);
PBL_AUDIO_CARD_DEFINE(s_mic, "mic", &s_mic_dai, &s_dmic, nullptr, 0, 0);

PBL_AUDIO_DAI_STATE_DEFINE(s_play_dai);
static const struct pbl_audio_dai s_play_dai =
    PBL_AUDIO_DAI_INIT(s_play_dai, "play_dai", nullptr, &s_playback_ops);
PBL_AUDIO_CARD_DEFINE(s_play, "play", &s_play_dai, nullptr, nullptr, 0, 0);

static void prv_period_done(const struct pbl_audio *audio, enum pbl_audio_dir dir, void *period,
                            const struct pbl_audio_pos *pos, void *arg) {
  cl_assert_equal_i(s_lock_depth, 0);
  s_periods[s_calls] = period;
  s_dirs[s_calls] = dir;
  s_pos[s_calls] = *pos;
  s_calls++;
  if (s_refill) {
    cl_assert_equal_i(pbl_audio_period_ready(audio, dir, period), 0);
  }
}

static struct pbl_audio_stream prv_stream(void) {
  return (struct pbl_audio_stream){
    .rate = RATE,
    .channels = 1U,
    .buf = s_ring,
    .period_frames = PERIOD_FRAMES,
    .periods = PERIODS,
    .period_done = prv_period_done,
  };
}

static uint8_t *prv_period(uint8_t idx) {
  return (uint8_t *)s_ring + idx * PERIOD_FRAMES * sizeof(int16_t);
}

void test_audio__initialize(void) {
  struct pbl_audio_stream stream = prv_stream();

  s_lock_depth = 0;
  s_ticks = 0U;
  memset(s_log, 0, sizeof(s_log));
  s_start_ret = 0;
  s_codec_enable_ret = 0;
  s_hw_gain = 1;
  s_hw_mute = false;
  s_codec_gain = 1;
  s_codec_mute = false;
  s_syncs = 0;
  s_pointer = 0U;
  s_dai_dir = UINT8_MAX;
  s_codec_dir = UINT8_MAX;
  s_codec_streaming = UINT8_MAX;
  memset(s_codec_gains, 0, sizeof(s_codec_gains));
  s_calls = 0;
  s_refill = true;
  memset(s_periods, 0, sizeof(s_periods));
  memset(s_pos, 0, sizeof(s_pos));

  memset(&s_out_device_state, 0, sizeof(s_out_device_state));
  memset(&s_in_device_state, 0, sizeof(s_in_device_state));
  memset(&s_linked_device_state, 0, sizeof(s_linked_device_state));
  memset(&s_mic_device_state, 0, sizeof(s_mic_device_state));
  memset(&s_out_dai_device_state, 0, sizeof(s_out_dai_device_state));
  memset(&s_in_dai_device_state, 0, sizeof(s_in_dai_device_state));
  memset(&s_linked_dai_device_state, 0, sizeof(s_linked_dai_device_state));
  memset(&s_mic_dai_device_state, 0, sizeof(s_mic_dai_device_state));
  memset(&s_play_device_state, 0, sizeof(s_play_device_state));
  memset(&s_play_dai_device_state, 0, sizeof(s_play_dai_device_state));
  memset(&s_dmic_device_state, 0, sizeof(s_dmic_device_state));
  memset(&s_codec_device_state, 0, sizeof(s_codec_device_state));
  memset(&s_amp_device_state, 0, sizeof(s_amp_device_state));
  cl_assert_equal_i(pbl_device_init(&s_out.dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_in.dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_linked.dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_mic.dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_play.dev), 0);

  s_ring = nullptr;
  stream.buf = nullptr;
  s_ring = pbl_audio_buf_alloc(pbl_audio_ring_bytes(&stream));
  cl_assert(s_ring != nullptr);
}

void test_audio__cleanup(void) {
  const struct pbl_audio *const cards[] = {&s_out, &s_in, &s_linked, &s_mic, &s_play};

  for (size_t i = 0U; i < sizeof(cards) / sizeof(cards[0]); i++) {
    for (uint8_t dir = 0U; dir < PBL_AUDIO_DIRS; dir++) {
      (void)pbl_audio_stop(cards[i], dir);
    }
  }
  pbl_audio_buf_free(s_ring);
}

void test_audio__buf_alloc(void) {
  struct pbl_audio_stream stream = prv_stream();

  cl_assert_equal_i(pbl_audio_ring_bytes(&stream), 64);
  cl_assert_equal_i((uintptr_t)s_ring % PBL_AUDIO_BUF_ALIGN, 0);
  for (size_t i = 0U; i < 64U; i++) {
    cl_assert_equal_i(((uint8_t *)s_ring)[i], 0);
  }
}

void test_audio__rate_bit(void) {
  cl_assert_equal_i(pbl_audio_rate_bit(16000U), PBL_AUDIO_RATE_16000);
  cl_assert_equal_i(pbl_audio_rate_bit(44100U), PBL_AUDIO_RATE_44100);
  cl_assert_equal_i(pbl_audio_rate_bit(16001U), 0);
}

void test_audio__start_validates(void) {
  struct pbl_audio_stream stream = prv_stream();

  stream.rate = 48000U;
  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), -EINVAL);

  stream = prv_stream();
  stream.periods = 1U;
  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), -EINVAL);

  stream = prv_stream();
  stream.periods = 5U;
  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), -EINVAL);

  stream = prv_stream();
  stream.channels = 3U;
  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), -EINVAL);

  stream = prv_stream();
  stream.buf = (uint8_t *)s_ring + 2;
  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), -EINVAL);

  cl_assert_equal_s(s_log, "");
  cl_assert(!pbl_audio_is_running(&s_out, PBL_AUDIO_PLAYBACK));

  stream = prv_stream();
  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), 0);
  cl_assert(pbl_audio_is_running(&s_out, PBL_AUDIO_PLAYBACK));
  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), -EBUSY);
}

void test_audio__start_failure(void) {
  struct pbl_audio_stream stream = prv_stream();

  s_start_ret = -EIO;
  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), -EIO);
  cl_assert(!pbl_audio_is_running(&s_out, PBL_AUDIO_PLAYBACK));

  pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);
  cl_assert_equal_i(s_calls, 0);
}

void test_audio__gain(void) {
  struct pbl_audio_stream stream = prv_stream();
  struct pbl_audio_caps caps;

  cl_assert_equal_i(pbl_audio_get_caps(&s_out, PBL_AUDIO_PLAYBACK, &caps), 0);
  cl_assert_equal_i(caps.gain_default_mdb, -6000);

  cl_assert_equal_i(pbl_audio_set_gain(&s_out, PBL_AUDIO_PLAYBACK, -12345), 0);
  cl_assert_equal_i(s_hw_gain, 1);

  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), 0);
  cl_assert_equal_i(s_hw_gain, -12500);

  cl_assert_equal_i(pbl_audio_set_gain(&s_out, PBL_AUDIO_PLAYBACK, 5000), 0);
  cl_assert_equal_i(s_hw_gain, 0);

  cl_assert_equal_i(pbl_audio_set_gain(&s_out, PBL_AUDIO_PLAYBACK, -99000), 0);
  cl_assert_equal_i(s_hw_gain, -36000);
}

void test_audio__mute_without_op_sets_lowest_gain(void) {
  struct pbl_audio_stream stream = prv_stream();

  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), 0);
  cl_assert_equal_i(s_hw_gain, -6000);

  cl_assert_equal_i(pbl_audio_set_mute(&s_out, PBL_AUDIO_PLAYBACK, true), 0);
  cl_assert_equal_i(s_hw_gain, -36000);

  cl_assert_equal_i(pbl_audio_set_mute(&s_out, PBL_AUDIO_PLAYBACK, false), 0);
  cl_assert_equal_i(s_hw_gain, -6000);
}

void test_audio__playback_positions_and_zero_fill(void) {
  struct pbl_audio_stream stream = prv_stream();

  memset(s_ring, 0x55, PERIODS * PERIOD_FRAMES * sizeof(int16_t));
  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), 0);

  s_ticks = PBL_TICK_HZ;
  pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);
  s_ticks = 2U * PBL_TICK_HZ;
  pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);
  pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);
  pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);

  cl_assert_equal_i(s_calls, 4);
  cl_assert(s_periods[0] == prv_period(0));
  cl_assert(s_periods[1] == prv_period(1));
  cl_assert(s_periods[2] == prv_period(2));
  cl_assert(s_periods[3] == prv_period(0));

  cl_assert_equal_i(s_pos[0].frame, PERIOD_FRAMES);
  cl_assert_equal_i(s_pos[0].ts_us, 1000000);
  cl_assert_equal_i(s_pos[1].frame, 2 * PERIOD_FRAMES);
  cl_assert_equal_i(s_pos[1].ts_us, 2000000);
  cl_assert_equal_i(s_pos[3].frame, 4 * PERIOD_FRAMES);
  for (int i = 0; i < 4; i++) {
    cl_assert(!s_pos[i].xrun);
  }

  for (size_t i = 0U; i < PERIODS * PERIOD_FRAMES * sizeof(int16_t); i++) {
    cl_assert_equal_i(((uint8_t *)s_ring)[i], 0);
  }
  cl_assert(s_syncs > 0);
}

void test_audio__playback_underrun(void) {
  struct pbl_audio_stream stream = prv_stream();

  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), 0);

  s_refill = false;
  pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);
  pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);
  cl_assert(!s_pos[0].xrun);
  cl_assert(!s_pos[1].xrun);

  // The hardware wraps back to period 0, which was never refilled
  pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);
  cl_assert(s_pos[2].xrun);

  cl_assert_equal_i(pbl_audio_period_ready(&s_out, PBL_AUDIO_PLAYBACK, prv_period(0)), 0);
  cl_assert_equal_i(pbl_audio_period_ready(&s_out, PBL_AUDIO_PLAYBACK, prv_period(1)), 0);
  cl_assert_equal_i(pbl_audio_period_ready(&s_out, PBL_AUDIO_PLAYBACK, prv_period(2)), 0);
  s_refill = true;
  pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);
  cl_assert(!s_pos[3].xrun);
}

void test_audio__period_ready_checks_ownership(void) {
  struct pbl_audio_stream stream = prv_stream();

  cl_assert_equal_i(pbl_audio_period_ready(&s_out, PBL_AUDIO_PLAYBACK, prv_period(0)), -EAGAIN);

  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), 0);
  cl_assert_equal_i(pbl_audio_period_ready(&s_out, PBL_AUDIO_PLAYBACK, prv_period(0)), -EINVAL);

  s_refill = false;
  pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);
  cl_assert_equal_i(pbl_audio_period_ready(&s_out, PBL_AUDIO_PLAYBACK, prv_period(0) + 2), -EINVAL);
  cl_assert_equal_i(pbl_audio_period_ready(&s_out, PBL_AUDIO_PLAYBACK, prv_period(3)), -EINVAL);
  cl_assert_equal_i(pbl_audio_period_ready(&s_out, PBL_AUDIO_PLAYBACK, prv_period(0)), 0);
  cl_assert_equal_i(pbl_audio_period_ready(&s_out, PBL_AUDIO_PLAYBACK, prv_period(0)), -EINVAL);
}

void test_audio__capture_overrun(void) {
  struct pbl_audio_stream stream = prv_stream();

  cl_assert_equal_i(pbl_audio_start(&s_in, PBL_AUDIO_CAPTURE, &stream), 0);

  s_refill = false;
  pbl_audio_period_elapsed(&s_in, PBL_AUDIO_CAPTURE);
  cl_assert(!s_pos[0].xrun);
  pbl_audio_period_elapsed(&s_in, PBL_AUDIO_CAPTURE);
  cl_assert(!s_pos[1].xrun);

  // Period 0 is still held while the hardware writes to it
  pbl_audio_period_elapsed(&s_in, PBL_AUDIO_CAPTURE);
  cl_assert(s_pos[2].xrun);

  cl_assert_equal_i(pbl_audio_period_ready(&s_in, PBL_AUDIO_CAPTURE, prv_period(0)), 0);
  cl_assert_equal_i(pbl_audio_period_ready(&s_in, PBL_AUDIO_CAPTURE, prv_period(1)), 0);
  cl_assert_equal_i(pbl_audio_period_ready(&s_in, PBL_AUDIO_CAPTURE, prv_period(2)), 0);
  s_refill = true;
  pbl_audio_period_elapsed(&s_in, PBL_AUDIO_CAPTURE);
  cl_assert(!s_pos[3].xrun);
}

void test_audio__reported_xrun(void) {
  struct pbl_audio_stream stream = prv_stream();

  cl_assert_equal_i(pbl_audio_start(&s_in, PBL_AUDIO_CAPTURE, &stream), 0);
  pbl_audio_report_xrun(&s_in, PBL_AUDIO_CAPTURE);
  pbl_audio_period_elapsed(&s_in, PBL_AUDIO_CAPTURE);
  pbl_audio_period_elapsed(&s_in, PBL_AUDIO_CAPTURE);
  cl_assert(s_pos[0].xrun);
  cl_assert(!s_pos[1].xrun);
}

void test_audio__stop(void) {
  struct pbl_audio_stream stream = prv_stream();

  cl_assert_equal_i(pbl_audio_stop(&s_out, PBL_AUDIO_PLAYBACK), 0);
  cl_assert_equal_s(s_log, "");

  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), 0);
  cl_assert_equal_i(pbl_audio_stop(&s_out, PBL_AUDIO_PLAYBACK), 0);
  cl_assert_equal_s(s_log, "Ss");
  cl_assert(!pbl_audio_is_running(&s_out, PBL_AUDIO_PLAYBACK));

  pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);
  cl_assert_equal_i(s_calls, 0);
}

void test_audio__position(void) {
  struct pbl_audio_stream stream = prv_stream();
  struct pbl_audio_pos pos;

  cl_assert_equal_i(pbl_audio_get_position(&s_out, PBL_AUDIO_PLAYBACK, &pos), -EAGAIN);

  s_ticks = PBL_TICK_HZ;
  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), 0);
  cl_assert_equal_i(pbl_audio_get_position(&s_out, PBL_AUDIO_PLAYBACK, &pos), 0);
  cl_assert_equal_i(pos.frame, 0);

  pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);
  s_pointer = PERIOD_FRAMES + 3U;
  s_ticks = 2U * PBL_TICK_HZ;
  cl_assert_equal_i(pbl_audio_get_position(&s_out, PBL_AUDIO_PLAYBACK, &pos), 0);
  cl_assert_equal_i(pos.frame, PERIOD_FRAMES + 3U);
  cl_assert_equal_i(pos.ts_us, 2000000);
}

void test_audio__position_ahead_of_interrupt(void) {
  struct pbl_audio_stream stream = prv_stream();
  struct pbl_audio_pos pos;

  cl_assert_equal_i(pbl_audio_start(&s_out, PBL_AUDIO_PLAYBACK, &stream), 0);

  s_pointer = PERIOD_FRAMES - 1U;
  cl_assert_equal_i(pbl_audio_get_position(&s_out, PBL_AUDIO_PLAYBACK, &pos), 0);
  cl_assert_equal_i(pos.frame, PERIOD_FRAMES - 1U);

  // Past the period end, before its interrupt
  s_pointer = PERIOD_FRAMES + 3U;
  cl_assert_equal_i(pbl_audio_get_position(&s_out, PBL_AUDIO_PLAYBACK, &pos), 0);
  cl_assert_equal_i(pos.frame, PERIOD_FRAMES + 3U);

  pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);
  cl_assert_equal_i(pbl_audio_get_position(&s_out, PBL_AUDIO_PLAYBACK, &pos), 0);
  cl_assert_equal_i(pos.frame, PERIOD_FRAMES + 3U);

  // Wrapped around the ring
  s_pointer = 1U;
  for (size_t i = 1U; i < PERIODS; i++) {
    pbl_audio_period_elapsed(&s_out, PBL_AUDIO_PLAYBACK);
  }
  cl_assert_equal_i(pbl_audio_get_position(&s_out, PBL_AUDIO_PLAYBACK, &pos), 0);
  cl_assert_equal_i(pos.frame, PERIODS * PERIOD_FRAMES + 1U);
}

void test_audio__link_sequencing(void) {
  struct pbl_audio_stream stream = prv_stream();

  cl_assert_equal_i(pbl_audio_start(&s_linked, PBL_AUDIO_PLAYBACK, &stream), 0);
  cl_assert_equal_i(pbl_audio_stop(&s_linked, PBL_AUDIO_PLAYBACK), 0);
  cl_assert_equal_s(s_log, "ASCasc");
}

void test_audio__link_routes_gain_to_codec(void) {
  struct pbl_audio_stream stream = prv_stream();
  struct pbl_audio_caps caps;

  cl_assert_equal_i(pbl_audio_get_caps(&s_linked, PBL_AUDIO_PLAYBACK, &caps), 0);
  cl_assert_equal_i(caps.gain_min_mdb, -78000);
  cl_assert_equal_i(caps.gain_step_mdb, 750);
  cl_assert_equal_i(caps.rates, PBL_AUDIO_RATE_16000);

  cl_assert_equal_i(pbl_audio_set_gain(&s_linked, PBL_AUDIO_PLAYBACK, -1000), 0);
  cl_assert_equal_i(pbl_audio_set_mute(&s_linked, PBL_AUDIO_PLAYBACK, true), 0);
  cl_assert_equal_i(pbl_audio_start(&s_linked, PBL_AUDIO_PLAYBACK, &stream), 0);
  cl_assert_equal_i(s_codec_gain, -1500);
  cl_assert(s_codec_mute);
  cl_assert_equal_i(s_hw_gain, 0);
  cl_assert(!s_hw_mute);

  cl_assert_equal_i(pbl_audio_set_gain(&s_linked, PBL_AUDIO_PLAYBACK, -48000), 0);
  cl_assert_equal_i(s_codec_gain, -48000);
  cl_assert_equal_i(pbl_audio_set_mute(&s_linked, PBL_AUDIO_PLAYBACK, false), 0);
  cl_assert(!s_codec_mute);
  cl_assert_equal_i(s_hw_gain, 0);
}

void test_audio__link_unwinds_on_codec_failure(void) {
  struct pbl_audio_stream stream = prv_stream();

  s_codec_enable_ret = -EIO;
  cl_assert_equal_i(pbl_audio_start(&s_linked, PBL_AUDIO_PLAYBACK, &stream), -EIO);
  cl_assert_equal_s(s_log, "ASCsa");
  cl_assert(!pbl_audio_is_running(&s_linked, PBL_AUDIO_PLAYBACK));
}

void test_audio__codec_without_gain_is_prepared_around_the_stream(void) {
  struct pbl_audio_stream stream = prv_stream();
  struct pbl_audio_caps caps;

  stream.buf = s_ring;
  cl_assert_equal_i(pbl_audio_get_caps(&s_mic, PBL_AUDIO_CAPTURE, &caps), 0);
  cl_assert(caps.gain_step_mdb != 0);

  cl_assert_equal_i(pbl_audio_set_gain(&s_mic, PBL_AUDIO_CAPTURE, -12000), 0);
  cl_assert_equal_i(pbl_audio_start(&s_mic, PBL_AUDIO_CAPTURE, &stream), 0);
  cl_assert_equal_s(s_log, "PS");
  cl_assert_equal_i(s_hw_gain, -12000);

  cl_assert_equal_i(pbl_audio_stop(&s_mic, PBL_AUDIO_CAPTURE), 0);
  cl_assert_equal_s(s_log, "PSsp");
}

void test_audio__both_directions_on_one_card(void) {
  struct pbl_audio_stream play = prv_stream();
  struct pbl_audio_stream rec = prv_stream();

  rec.buf = pbl_audio_buf_alloc(pbl_audio_ring_bytes(&rec));
  cl_assert(rec.buf != nullptr);

  // Playback powers the amplifier; the codec is told nothing else streams
  cl_assert_equal_i(pbl_audio_start(&s_linked, PBL_AUDIO_PLAYBACK, &play), 0);
  cl_assert_equal_s(s_log, "ASC");
  cl_assert_equal_i(s_dai_dir, PBL_AUDIO_PLAYBACK);
  cl_assert_equal_i(s_codec_dir, PBL_AUDIO_PLAYBACK);
  cl_assert_equal_i(s_codec_streaming, 0);

  // Capture joins on the same card, without the amplifier, told playback streams
  cl_assert_equal_i(pbl_audio_start(&s_linked, PBL_AUDIO_CAPTURE, &rec), 0);
  cl_assert_equal_s(s_log, "ASCSC");
  cl_assert_equal_i(s_dai_dir, PBL_AUDIO_CAPTURE);
  cl_assert_equal_i(s_codec_dir, PBL_AUDIO_CAPTURE);
  cl_assert_equal_i(s_codec_streaming, PBL_BIT(PBL_AUDIO_PLAYBACK));
  cl_assert(pbl_audio_is_running(&s_linked, PBL_AUDIO_PLAYBACK));
  cl_assert(pbl_audio_is_running(&s_linked, PBL_AUDIO_CAPTURE));
  cl_assert_equal_i(pbl_audio_start(&s_linked, PBL_AUDIO_CAPTURE, &rec), -EBUSY);

  // Each direction has its own ring and position
  pbl_audio_period_elapsed(&s_linked, PBL_AUDIO_CAPTURE);
  pbl_audio_period_elapsed(&s_linked, PBL_AUDIO_PLAYBACK);
  pbl_audio_period_elapsed(&s_linked, PBL_AUDIO_CAPTURE);
  cl_assert_equal_i(s_calls, 3);
  cl_assert_equal_i(s_dirs[0], PBL_AUDIO_CAPTURE);
  cl_assert(s_periods[0] == rec.buf);
  cl_assert_equal_i(s_pos[0].frame, PERIOD_FRAMES);
  cl_assert_equal_i(s_dirs[1], PBL_AUDIO_PLAYBACK);
  cl_assert(s_periods[1] == prv_period(0));
  cl_assert_equal_i(s_pos[1].frame, PERIOD_FRAMES);
  cl_assert_equal_i(s_dirs[2], PBL_AUDIO_CAPTURE);
  cl_assert_equal_i(s_pos[2].frame, 2 * PERIOD_FRAMES);

  // Playback leaves: the codec keeps the capture path
  cl_assert_equal_i(pbl_audio_stop(&s_linked, PBL_AUDIO_PLAYBACK), 0);
  cl_assert_equal_s(s_log, "ASCSCasc");
  cl_assert_equal_i(s_dai_dir, PBL_AUDIO_PLAYBACK);
  cl_assert_equal_i(s_codec_dir, PBL_AUDIO_PLAYBACK);
  cl_assert_equal_i(s_codec_streaming, PBL_BIT(PBL_AUDIO_CAPTURE));
  cl_assert(!pbl_audio_is_running(&s_linked, PBL_AUDIO_PLAYBACK));
  cl_assert(pbl_audio_is_running(&s_linked, PBL_AUDIO_CAPTURE));

  pbl_audio_period_elapsed(&s_linked, PBL_AUDIO_PLAYBACK);
  pbl_audio_period_elapsed(&s_linked, PBL_AUDIO_CAPTURE);
  cl_assert_equal_i(s_calls, 4);
  cl_assert_equal_i(s_dirs[3], PBL_AUDIO_CAPTURE);

  // The last one powers the codec down
  cl_assert_equal_i(pbl_audio_stop(&s_linked, PBL_AUDIO_CAPTURE), 0);
  cl_assert_equal_s(s_log, "ASCSCascsc");
  cl_assert_equal_i(s_codec_dir, PBL_AUDIO_CAPTURE);
  cl_assert_equal_i(s_codec_streaming, 0);

  pbl_audio_buf_free(rec.buf);
}

void test_audio__gain_and_mute_per_direction(void) {
  struct pbl_audio_stream play = prv_stream();
  struct pbl_audio_stream rec = prv_stream();

  rec.buf = pbl_audio_buf_alloc(pbl_audio_ring_bytes(&rec));
  cl_assert(rec.buf != nullptr);

  cl_assert_equal_i(pbl_audio_set_gain(&s_linked, PBL_AUDIO_PLAYBACK, -1500), 0);
  cl_assert_equal_i(pbl_audio_set_gain(&s_linked, PBL_AUDIO_CAPTURE, 6000), 0);
  cl_assert_equal_i(pbl_audio_set_mute(&s_linked, PBL_AUDIO_CAPTURE, true), 0);

  cl_assert_equal_i(pbl_audio_start(&s_linked, PBL_AUDIO_PLAYBACK, &play), 0);
  cl_assert(!s_codec_mute);
  cl_assert_equal_i(pbl_audio_start(&s_linked, PBL_AUDIO_CAPTURE, &rec), 0);
  cl_assert(s_codec_mute);
  cl_assert_equal_i(s_codec_gains[PBL_AUDIO_PLAYBACK], -1500);
  cl_assert_equal_i(s_codec_gains[PBL_AUDIO_CAPTURE], 6000);

  cl_assert_equal_i(pbl_audio_set_gain(&s_linked, PBL_AUDIO_CAPTURE, -3000), 0);
  cl_assert_equal_i(s_codec_gains[PBL_AUDIO_PLAYBACK], -1500);
  cl_assert_equal_i(s_codec_gains[PBL_AUDIO_CAPTURE], -3000);

  pbl_audio_buf_free(rec.buf);
}

void test_audio__direction_the_card_does_not_stream(void) {
  struct pbl_audio_stream stream = prv_stream();
  struct pbl_audio_caps caps;

  cl_assert_equal_i(pbl_audio_get_caps(&s_play, PBL_AUDIO_CAPTURE, &caps), -ENOTSUP);
  cl_assert_equal_i(pbl_audio_start(&s_play, PBL_AUDIO_CAPTURE, &stream), -ENOTSUP);
  cl_assert_equal_i(pbl_audio_set_gain(&s_play, PBL_AUDIO_CAPTURE, 0), -ENOTSUP);
  cl_assert_equal_i(pbl_audio_set_mute(&s_play, PBL_AUDIO_CAPTURE, true), -ENOTSUP);
  cl_assert_equal_i(pbl_audio_start(&s_play, PBL_AUDIO_DIRS, &stream), -ENOTSUP);
  cl_assert(!pbl_audio_is_running(&s_play, PBL_AUDIO_DIRS));
  cl_assert_equal_s(s_log, "");

  cl_assert_equal_i(pbl_audio_get_caps(&s_play, PBL_AUDIO_PLAYBACK, &caps), 0);
  cl_assert_equal_i(pbl_audio_start(&s_play, PBL_AUDIO_PLAYBACK, &stream), 0);
  cl_assert_equal_s(s_log, "S");
}

void test_audio__capture_failure_leaves_playback_running(void) {
  struct pbl_audio_stream play = prv_stream();
  struct pbl_audio_stream rec = prv_stream();

  rec.buf = pbl_audio_buf_alloc(pbl_audio_ring_bytes(&rec));
  cl_assert(rec.buf != nullptr);

  cl_assert_equal_i(pbl_audio_start(&s_linked, PBL_AUDIO_PLAYBACK, &play), 0);
  s_codec_enable_ret = -EIO;
  cl_assert_equal_i(pbl_audio_start(&s_linked, PBL_AUDIO_CAPTURE, &rec), -EIO);
  cl_assert_equal_s(s_log, "ASCSCs");
  cl_assert_equal_i(s_dai_dir, PBL_AUDIO_CAPTURE);
  cl_assert(pbl_audio_is_running(&s_linked, PBL_AUDIO_PLAYBACK));
  cl_assert(!pbl_audio_is_running(&s_linked, PBL_AUDIO_CAPTURE));

  pbl_audio_buf_free(rec.buf);
}
