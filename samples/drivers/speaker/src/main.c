/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/audio.h>
#include <pbl/drivers/charger.h>
#include <pbl/kernel/sem.h>
#include <pbl/util/size.h>
#include <pbl/util/trig.h>

#define RATE      16000U
#define PERIOD_MS 20U
#define PERIODS   2U
#define NOTE_MS   200U
#define AMPLITUDE (INT16_MAX / 4)

#define PERIOD_FRAMES (RATE * PERIOD_MS / 1000U)
#define NOTE_FRAMES   (RATE * NOTE_MS / 1000U)

/* C major arpeggio, in Hz. */
static const uint32_t s_notes[] = {523U, 659U, 784U, 1047U};

/* The stream state, written only by the period callback once it starts. */
static struct {
  struct pbl_sem done;
  uint32_t frame;
  uint32_t phase;
  uint32_t tail;
  uint32_t xruns;
} s_play = {
  .done = PBL_SEM_INITIALIZER(0, 1),
};

static void prv_fill(int16_t *samples) {
  for (size_t i = 0U; i < PERIOD_FRAMES; i++, s_play.frame++) {
    size_t note = s_play.frame / NOTE_FRAMES;

    if (note >= PBL_ARRAY_SIZE(s_notes)) {
      samples[i] = 0;
      continue;
    }

    /* Phase in 1/65536 turns, scaled by 2^16 to keep the fraction. */
    s_play.phase += (uint32_t)(((uint64_t)s_notes[note] * TRIG_MAX_ANGLE << 16) / RATE);
    samples[i] = (int16_t)(sin_lookup((int32_t)(s_play.phase >> 16)) * AMPLITUDE / TRIG_MAX_RATIO);
  }
}

/* Runs in an interrupt, each time the hardware is done with a period: refill it and hand it back.
 */
static void prv_period_done(const struct pbl_audio *audio, void *period,
                            const struct pbl_audio_pos *pos, void *arg) {
  prv_fill(period);
  s_play.xruns += pos->xrun ? 1U : 0U;
  (void)pbl_audio_period_ready(audio, period);

  /* Done once the whole ring holds silence: the last notes have played out. */
  if (s_play.frame >= PBL_ARRAY_SIZE(s_notes) * NOTE_FRAMES && ++s_play.tail == PERIODS + 1U) {
    pbl_sem_give(&s_play.done);
  }
}

int main(void) {
  struct pbl_audio_stream stream = {
    .rate = RATE,
    .channels = 1U,
    .period_frames = PERIOD_FRAMES,
    .periods = PERIODS,
    .period_done = prv_period_done,
  };
  struct pbl_audio_caps caps;
  int ret;

  ret = pbl_audio_get_caps(SPEAKER, &caps);
  if (ret != 0) {
    fprintf(stderr, "speaker: not ready (%d)\n", ret);
    return ret;
  }

  if ((caps.rates & pbl_audio_rate_bit(RATE)) == 0U || caps.min_channels > 1U) {
    fprintf(stderr, "speaker: cannot play mono at %d Hz\n", (int)RATE);
    return -ENOTSUP;
  }

  ret = pbl_audio_set_gain(SPEAKER, caps.gain_default_mdb);
  if (ret == 0) {
    ret = pbl_audio_set_mute(SPEAKER, false);
  }
  if (ret != 0) {
    fprintf(stderr, "speaker: gain failed (%d)\n", ret);
    return ret;
  }

  /* The ring must be reachable by DMA: allocate it with the class. */
  stream.buf = pbl_audio_buf_alloc(pbl_audio_ring_bytes(&stream));
  if (stream.buf == nullptr) {
    fprintf(stderr, "speaker: out of memory\n");
    return -ENOMEM;
  }

  /* Fill the whole ring before starting: the hardware plays it right away. */
  for (uint8_t i = 0U; i < PERIODS; i++) {
    prv_fill((int16_t *)stream.buf + i * PERIOD_FRAMES);
  }

  printf("speaker: playing %d notes at %d Hz\n", (int)PBL_ARRAY_SIZE(s_notes), (int)RATE);

#ifdef CONFIG_CHARGER_HIGH_LOAD
  /* The speaker draws more than the battery gives by default. */
  (void)pbl_charger_high_load_request(CHARGER);
#endif

  ret = pbl_audio_start(SPEAKER, &stream);
  if (ret == 0) {
    ret = pbl_sem_take(&s_play.done, PBL_MSEC(PBL_ARRAY_SIZE(s_notes) * NOTE_MS + 1000U));
    (void)pbl_audio_stop(SPEAKER);
  }

#ifdef CONFIG_CHARGER_HIGH_LOAD
  (void)pbl_charger_high_load_release(CHARGER);
#endif

  pbl_audio_buf_free(stream.buf);

  if (ret != 0) {
    fprintf(stderr, "speaker: playback failed (%d)\n", ret);
    return ret;
  }

  printf("speaker: done, %d xruns\n", (int)s_play.xruns);
  printf("Sample done\n");

  return 0;
}
