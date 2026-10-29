/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>

#include <pbl/drivers/audio.h>
#include <pbl/kernel/sem.h>
#include <pbl/shell/shell.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>
#include <pbl/util/trig.h>

#define MS_PER_S       1000U
#define PERIOD_MS      20U
#define PERIODS        2U
#define MAX_CHANNELS   2U
#define MAX_RECORD_MS  10000UL
#define MAX_TONE_HZ    20000UL
#define TONE_AMPLITUDE (INT16_MAX / 2)
#define STOP_SLACK_MS  (PERIODS * PERIOD_MS * 2U)

struct prv_session {
  struct pbl_sem done;
  uint32_t rate;
  uint8_t channels;
  size_t period_frames;
  uint32_t frames_left;
  uint32_t frames;
  uint32_t xruns;
  uint32_t phase;
  uint32_t phase_inc;
  int16_t peak[MAX_CHANNELS];
  uint64_t sum_sq[MAX_CHANNELS];
};

static const char *prv_dir_name(const struct pbl_audio *audio) {
  return audio->dir == PBL_AUDIO_PLAYBACK ? "playback" : "capture";
}

static uint32_t prv_pick_rate(const struct pbl_audio_caps *caps) {
  static const uint32_t preferred[] = {16000U, 48000U, 8000U};

  for (size_t i = 0U; i < ARRAY_LENGTH(preferred); i++) {
    if ((pbl_audio_rate_bit(preferred[i]) & caps->rates) != 0U) {
      return preferred[i];
    }
  }

  return 0U;
}

static const struct pbl_audio *prv_find(const struct pbl_shell *sh, const char *name,
                                        enum pbl_audio_dir dir) {
  const struct pbl_device *dev = pbl_device_find(&pbl_audio_class, name);
  const struct pbl_audio *audio;

  if (dev == nullptr) {
    pbl_shell_error(sh, "unknown device '%s'", name);
    return nullptr;
  }

  audio = container_of(dev, const struct pbl_audio, dev);
  if (audio->dir != dir) {
    pbl_shell_error(sh, "%s is not a %s device", name,
                    dir == PBL_AUDIO_PLAYBACK ? "playback" : "capture");
    return nullptr;
  }

  return audio;
}

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_audio_class, dev) {
    const struct pbl_audio *audio = container_of(dev, const struct pbl_audio, dev);
    struct pbl_audio_caps caps;

    if (pbl_audio_get_caps(audio, &caps) != 0) {
      pbl_shell_print(sh, "%s: %s (not ready)", dev->name, prv_dir_name(audio));
      continue;
    }

    pbl_shell_print(sh,
                    "%s: %s rates 0x%" PRIx32 " channels %u-%u gain %" PRId32 "..%" PRId32
                    " mdB step %" PRId32 "%s",
                    dev->name, prv_dir_name(audio), caps.rates, caps.min_channels,
                    caps.max_channels, caps.gain_min_mdb, caps.gain_max_mdb, caps.gain_step_mdb,
                    pbl_audio_is_running(audio) ? " (running)" : "");
  }

  return 0;
}

static void prv_record_done(const struct pbl_audio *audio, void *period,
                            const struct pbl_audio_pos *pos, void *arg) {
  struct prv_session *s = arg;
  const int16_t *samples = period;
  size_t frames = MIN(s->period_frames, s->frames_left);

  for (size_t i = 0U; i < frames; i++) {
    for (uint8_t ch = 0U; ch < s->channels; ch++) {
      int32_t v = samples[i * s->channels + ch];

      s->peak[ch] = MAX(s->peak[ch], (int16_t)MIN(ABS(v), INT16_MAX));
      s->sum_sq[ch] += (uint64_t)(v * v);
    }
  }

  s->xruns += pos->xrun ? 1U : 0U;
  s->frames += frames;
  s->frames_left -= frames;
  (void)pbl_audio_period_ready(audio, period);

  if (s->frames_left == 0U && frames > 0U) {
    pbl_sem_give(&s->done);
  }
}

static void prv_tone_fill(struct prv_session *s, int16_t *samples) {
  size_t frames = MIN(s->period_frames, s->frames_left);

  for (size_t i = 0U; i < frames; i++) {
    int16_t v = (int16_t)(sin_lookup((int32_t)(s->phase >> 16)) * TONE_AMPLITUDE / TRIG_MAX_RATIO);

    for (uint8_t ch = 0U; ch < s->channels; ch++) {
      samples[i * s->channels + ch] = v;
    }
    s->phase += s->phase_inc;
  }

  s->frames_left -= frames;
  if (frames == 0U) {
    s->frames++;
  }
}

static void prv_tone_done(const struct pbl_audio *audio, void *period,
                          const struct pbl_audio_pos *pos, void *arg) {
  struct prv_session *s = arg;

  prv_tone_fill(s, period);
  s->xruns += pos->xrun ? 1U : 0U;
  (void)pbl_audio_period_ready(audio, period);

  // Done once the ring has played out the last tone samples
  if (s->frames == PERIODS) {
    pbl_sem_give(&s->done);
  }
}

static int prv_run(const struct pbl_shell *sh, const struct pbl_audio *audio, struct prv_session *s,
                   pbl_audio_period_cb cb, uint32_t ms) {
  struct pbl_audio_stream stream = {
    .rate = s->rate,
    .channels = s->channels,
    .period_frames = s->period_frames,
    .periods = PERIODS,
    .period_done = cb,
    .arg = s,
  };
  int ret;

  stream.buf = pbl_audio_buf_alloc(pbl_audio_ring_bytes(&stream));
  if (stream.buf == nullptr) {
    pbl_shell_error(sh, "out of memory");
    return -ENOMEM;
  }

  if (audio->dir == PBL_AUDIO_PLAYBACK) {
    for (uint8_t i = 0U; i < PERIODS; i++) {
      prv_tone_fill(s, (int16_t *)stream.buf + i * s->period_frames * s->channels);
    }
  }

  pbl_sem_init(&s->done, 0, 1);

  ret = pbl_audio_start(audio, &stream);
  if (ret != 0) {
    pbl_shell_error(sh, "start failed (%d)", ret);
  } else {
    if (pbl_sem_take(&s->done, PBL_MSEC(ms + STOP_SLACK_MS)) != 0) {
      pbl_shell_error(sh, "timed out");
      ret = -ETIMEDOUT;
    }
    (void)pbl_audio_stop(audio);
  }

  pbl_sem_deinit(&s->done);
  pbl_audio_buf_free(stream.buf);

  return ret;
}

static int prv_setup(const struct pbl_shell *sh, const struct pbl_audio *audio,
                     struct prv_session *s, uint8_t channels) {
  struct pbl_audio_caps caps;
  int ret;

  ret = pbl_audio_get_caps(audio, &caps);
  if (ret != 0) {
    pbl_shell_error(sh, "device not ready");
    return ret;
  }

  *s = (struct prv_session){
    .rate = prv_pick_rate(&caps),
    .channels = channels != 0U ? channels : MIN(caps.max_channels, MAX_CHANNELS),
  };
  s->period_frames = s->rate * PERIOD_MS / MS_PER_S;

  if (s->rate == 0U || s->channels < caps.min_channels || s->channels > caps.max_channels ||
      s->channels > MAX_CHANNELS) {
    pbl_shell_error(sh, "no supported format");
    return -EINVAL;
  }

  return 0;
}

static int prv_cmd_rec(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_audio *audio = prv_find(sh, argv[1], PBL_AUDIO_CAPTURE);
  unsigned long channels = 0UL;
  unsigned long ms;
  struct prv_session s;
  int ret;

  if (audio == nullptr) {
    return -ENODEV;
  }

  if (pbl_shell_strtoul(argv[2], &ms) != 0 || ms == 0UL || ms > MAX_RECORD_MS) {
    pbl_shell_error(sh, "invalid duration '%s'", argv[2]);
    return -EINVAL;
  }

  if (argc > 3 &&
      (pbl_shell_strtoul(argv[3], &channels) != 0 || channels == 0UL || channels > MAX_CHANNELS)) {
    pbl_shell_error(sh, "invalid channels '%s'", argv[3]);
    return -EINVAL;
  }

  ret = prv_setup(sh, audio, &s, (uint8_t)channels);
  if (ret != 0) {
    return ret;
  }

  s.frames_left = s.rate * ms / MS_PER_S;

  ret = prv_run(sh, audio, &s, prv_record_done, ms);
  if (ret != 0) {
    return ret;
  }

  pbl_shell_print(sh, "%" PRIu32 " frames at %" PRIu32 " Hz, %" PRIu32 " xruns", s.frames, s.rate,
                  s.xruns);
  for (uint8_t ch = 0U; ch < s.channels; ch++) {
    pbl_shell_print(sh, "ch%u: peak %d rms %" PRId32, ch, s.peak[ch],
                    integer_sqrt((int64_t)(s.sum_sq[ch] / MAX(s.frames, 1U))));
  }

  return 0;
}

static int prv_cmd_tone(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_audio *audio = prv_find(sh, argv[1], PBL_AUDIO_PLAYBACK);
  unsigned long hz;
  unsigned long ms;
  long mdb;
  struct prv_session s;
  int ret;

  if (audio == nullptr) {
    return -ENODEV;
  }

  if (pbl_shell_strtoul(argv[2], &hz) != 0 || hz == 0UL || hz > MAX_TONE_HZ) {
    pbl_shell_error(sh, "invalid frequency '%s'", argv[2]);
    return -EINVAL;
  }

  if (pbl_shell_strtoul(argv[3], &ms) != 0 || ms == 0UL || ms > MAX_RECORD_MS) {
    pbl_shell_error(sh, "invalid duration '%s'", argv[3]);
    return -EINVAL;
  }

  if (pbl_shell_strtol(argv[4], &mdb) != 0) {
    pbl_shell_error(sh, "invalid gain '%s'", argv[4]);
    return -EINVAL;
  }

  ret = prv_setup(sh, audio, &s, 0U);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_audio_set_gain(audio, (int32_t)mdb);
  if (ret == 0) {
    ret = pbl_audio_set_mute(audio, false);
  }
  if (ret != 0) {
    pbl_shell_error(sh, "gain failed (%d)", ret);
    return ret;
  }

  s.frames_left = s.rate * ms / MS_PER_S;
  s.phase_inc = (uint32_t)(((uint64_t)hz * TRIG_MAX_ANGLE << 16) / s.rate);

  ret = prv_run(sh, audio, &s, prv_tone_done, ms);
  if (ret != 0) {
    return ret;
  }

  pbl_shell_print(sh, "played %lu Hz for %lu ms, %" PRIu32 " xruns", hz, ms, s.xruns);

  return 0;
}

static const struct pbl_shell_cmd sub_audio[] = {
  PBL_SHELL_CMD(list, nullptr, "List the devices", prv_cmd_list),
  PBL_SHELL_CMD_ARG(rec, nullptr, "Record and print peak and RMS <dev> <ms> [channels]",
                    prv_cmd_rec, 3, 1),
  PBL_SHELL_CMD_ARG(tone, nullptr, "Play a sine <dev> <hz> <ms> <gain mdB>", prv_cmd_tone, 5, 0),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(audio, sub_audio, "Audio devices", nullptr);
