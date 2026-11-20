/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/audio.h>
#include <pbl/kernel/msgq.h>
#include <pbl/util/math.h>

#define RATE      16000U
#define PERIOD_MS 20U
#define PERIODS   2U
#define WINDOW_MS 200U
#define WINDOWS   10U

#define PERIOD_FRAMES  (RATE * PERIOD_MS / 1000U)
#define WINDOW_PERIODS (WINDOW_MS / PERIOD_MS)

struct level {
  int32_t peak;
  int32_t rms;
  bool xrun;
};

/* The callback runs in an interrupt: it measures and queues, the main thread prints. */
static PBL_MSGQ_DEFINE(s_levels, sizeof(struct level), WINDOWS);

static struct {
  uint32_t periods;
  int32_t peak;
  uint64_t sum_sq;
  bool xrun;
} s_win;

static void prv_period_done(const struct pbl_audio *audio, void *period,
                            const struct pbl_audio_pos *pos, void *arg) {
  const int16_t *samples = period;

  for (size_t i = 0U; i < PERIOD_FRAMES; i++) {
    int32_t v = samples[i];

    s_win.peak = PBL_MAX(s_win.peak, PBL_ABS(v));
    s_win.sum_sq += (uint64_t)(v * v);
  }
  s_win.xrun |= pos->xrun;

  /* Done with the period: the hardware may fill it again. */
  (void)pbl_audio_period_ready(audio, period);

  if (++s_win.periods == WINDOW_PERIODS) {
    struct level level = {
      .peak = s_win.peak,
      .rms = integer_sqrt((int64_t)(s_win.sum_sq / (WINDOW_PERIODS * PERIOD_FRAMES))),
      .xrun = s_win.xrun,
    };

    (void)pbl_msgq_put(&s_levels, &level, PBL_NO_WAIT);
    s_win.periods = 0U;
    s_win.peak = 0;
    s_win.sum_sq = 0U;
    s_win.xrun = false;
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

  ret = pbl_audio_get_caps(MIC, &caps);
  if (ret != 0) {
    fprintf(stderr, "mic: not ready (%d)\n", ret);
    return ret;
  }

  if ((caps.rates & pbl_audio_rate_bit(RATE)) == 0U || caps.min_channels > 1U) {
    fprintf(stderr, "mic: cannot capture mono at %d Hz\n", (int)RATE);
    return -ENOTSUP;
  }

  /* The ring must be reachable by DMA: allocate it with the class. */
  stream.buf = pbl_audio_buf_alloc(pbl_audio_ring_bytes(&stream));
  if (stream.buf == nullptr) {
    fprintf(stderr, "mic: out of memory\n");
    return -ENOMEM;
  }

  printf("mic: capturing %d ms at %d Hz\n", (int)(WINDOWS * WINDOW_MS), (int)RATE);

  ret = pbl_audio_start(MIC, &stream);
  for (unsigned int i = 0U; ret == 0 && i < WINDOWS; i++) {
    struct level level;

    ret = pbl_msgq_get(&s_levels, &level, PBL_MSEC(WINDOW_MS * 4U));
    if (ret == 0) {
      printf("mic: peak %d, rms %d%s\n", (int)level.peak, (int)level.rms,
             level.xrun ? ", overrun" : "");
    }
  }

  (void)pbl_audio_stop(MIC);
  pbl_audio_buf_free(stream.buf);

  if (ret != 0) {
    fprintf(stderr, "mic: capture failed (%d)\n", ret);
    return ret;
  }

  printf("Sample done\n");

  return 0;
}
