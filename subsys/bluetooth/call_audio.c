/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "call_audio_cvsd.h"
#include "call_audio_dsp.h"

#include <string.h>

#include <pbl/bluetooth/call_audio.h>
#include <pbl/bluetooth/lcpu_audio_probe.h>
#include <pbl/drivers/mic.h>
#include <pbl/kernel/debug.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/services/speaker/speaker_service.h>

#include <board/board.h>
#include <kernel/pbl_malloc.h>

#define STACK_SIZE 2048
// Below the stack, in the same allocation: a pattern the thread checks, so an overflow is caught
// before it reaches the kernel heap's own data (the heap walks it with interrupts masked).
#define STACK_GUARD_WORDS 16
#define STACK_GUARD_WORD  0xCA11AD10U
// 15 ms of 16 kHz audio: two packets towards the phone.
#define MIC_FRAME_SAMPLES 240
// The speaker service maps 0-100 to -36..0 dB (after the user's speaker volume); past 100 the
// phone's audio is multiplied by 1 << s_rx_shift, saturating. Kept from call to call.
#define VOLUME_DEFAULT 90
#define VOLUME_STEP    10
#define VOLUME_LOWEST  30
#define RX_SHIFT_MAX   2
// The microphone's own gain (mic_set_volume(), 0-100) for the call, and after it (the driver's
// default, 90 of 120); then a gain of 1 << s_mic_shift on the 8 kHz samples, saturating.
#define MIC_CALL_GAIN    100
#define MIC_DEFAULT_GAIN 75
#define MIC_SHIFT_MAX    3
// Instead of a fixed gain: the automatic one.
#define MIC_SHIFT_AGC   (MIC_SHIFT_MAX + 1)
#define LEVEL_PERIOD_MS 1000
// Silence put in the speaker's queue at the start, as a cushion.
#define PREFILL_PACKETS 2

static PBL_MUTEX_DEFINE(s_lock);
static PBL_SEM_DEFINE(s_wake, 0, 1);
static struct pbl_thread s_thread;
static uint32_t *s_stack;
static int16_t *s_mic_buf;
// What the call needs only while it lasts, allocated for it.
struct call_audio_work {
  struct call_audio_cvsd cvsd;
  struct call_audio_halfband down;
  struct call_audio_halfband up;
  struct call_audio_agc agc;
  struct call_audio_es es;
  int16_t spk[CALL_AUDIO_SPK_SAMPLES];
};
static struct call_audio_work *s_work;
static volatile bool s_quit;
static uint8_t s_volume = VOLUME_DEFAULT;
static uint8_t s_mic_shift = MIC_SHIFT_AGC;
static uint8_t s_rx_shift = 1;
static struct call_audio_stats s_stats;

// Working state of the audio thread.
static struct {
  int16_t tx_pcm[CALL_AUDIO_PCM_SAMPLES];
  size_t tx_fill;
  struct call_audio_level rx_level;
  struct call_audio_level tx_level;
  struct call_audio_steps rx_steps;
  // The LCPU converts the CVSD itself (it sent PCM): send it PCM too.
  bool lcpu_pcm;
  uint64_t last_rx_ms;
  uint64_t level_ms;
} s_w;

static const int16_t s_silence[CALL_AUDIO_SPK_SAMPLES];

// From the IPC and microphone interrupts.
static void prv_wake(void *context) {
  pbl_sem_give(&s_wake);
}

// The longest a conversion took, in microseconds.
static void prv_note_us(uint16_t *max_us, uint32_t start) {
  const uint64_t us = (uint64_t)(pbl_cycle_get_32() - start) * 1000000U / pbl_cycles_per_sec();
  if (us > *max_us) {
    *max_us = (uint16_t)(us > UINT16_MAX ? UINT16_MAX : us);
  }
}

/* Towards the phone */

static void prv_tx_packet(void) {
  uint32_t air[CALL_AUDIO_AIR_BYTES / 4];
  const uint8_t *out = (const uint8_t *)s_w.tx_pcm;
  size_t len = CALL_AUDIO_PCM_BYTES;
  if (!s_w.lcpu_pcm) {
    const uint32_t start = pbl_cycle_get_32();
    call_audio_cvsd_encode(&s_work->cvsd, s_w.tx_pcm, (uint8_t *)air);
    prv_note_us(&s_stats.tx_cvsd_us, start);
    out = (const uint8_t *)air;
    len = CALL_AUDIO_AIR_BYTES;
  }
  if (lcpu_audio_tx_write(out, len) != 0) {
    s_stats.tx_packets++;
  } else {
    s_stats.tx_overflows++;
  }
}

// From mic_poll(), on the audio thread.
static void prv_mic_frame(int16_t *samples, size_t sample_count, void *context) {
  int16_t half[MIC_FRAME_SAMPLES / 2];
  const size_t n = sample_count / 2;
  call_audio_down2(&s_work->down, samples, n * 2, half);
  if (s_mic_shift == MIC_SHIFT_AGC) {
    // What the mic hears while the far end talks is mostly its echo: no gain for it.
    s_work->agc.hold = call_audio_es_far_active(&s_work->es);
    call_audio_agc_run(&s_work->agc, half, n);
  } else {
    call_audio_gain(half, n, s_mic_shift);
  }
  call_audio_es_near(&s_work->es, half, n);
  call_audio_level_add(&s_w.tx_level, half, n);
  s_stats.mic_frames++;
  for (size_t i = 0; i < n; i++) {
    s_w.tx_pcm[s_w.tx_fill++] = half[i];
    if (s_w.tx_fill == CALL_AUDIO_PCM_SAMPLES) {
      prv_tx_packet();
      s_w.tx_fill = 0;
    }
  }
}

/* From the phone */

static void prv_flush_rx(void) {
  uint32_t pkt[CALL_AUDIO_RX_BYTES / 4];
  while (lcpu_audio_rx_packet((uint8_t *)pkt, sizeof(pkt)) != 0) {
  }
}

static void prv_speaker_write(const void *pcm) {
  if (speaker_service_stream_write_owned(PebbleTask_Unknown, pcm, CALL_AUDIO_SPK_BYTES) <
      CALL_AUDIO_SPK_BYTES) {
    s_stats.rx_short++;
  }
}

static void prv_rx_packet(const uint8_t *pkt, size_t len) {
  memcpy(s_stats.rx_hdr, pkt, sizeof(s_stats.rx_hdr));
  if (!call_audio_rx_header_ok(pkt, len)) {
    // Out of step with the packets: start again from an empty ring, as the SDK does.
    s_stats.rx_bad++;
    prv_flush_rx();
    return;
  }
  s_stats.rx_packets++;

  const uint64_t now = pbl_uptime_ms();
  const uint32_t gap = s_w.last_rx_ms ? (uint32_t)(now - s_w.last_rx_ms) : 0;
  s_w.last_rx_ms = now;
  if (gap > s_stats.rx_max_gap_ms) {
    s_stats.rx_max_gap_ms = (uint16_t)(gap > UINT16_MAX ? UINT16_MAX : gap);
  }

  int16_t decoded[CALL_AUDIO_PCM_SAMPLES];
  const int16_t *pcm = (const int16_t *)&pkt[CALL_AUDIO_RX_HDR_BYTES];
  s_w.lcpu_pcm = pkt[0] == CALL_AUDIO_PCM_BYTES;
  if (!s_w.lcpu_pcm) {
    const uint32_t start = pbl_cycle_get_32();
    call_audio_cvsd_decode(&s_work->cvsd, &pkt[CALL_AUDIO_RX_HDR_BYTES], decoded);
    prv_note_us(&s_stats.rx_cvsd_us, start);
    pcm = decoded;
  }
  call_audio_steps_add(&s_w.rx_steps, pcm, CALL_AUDIO_PCM_SAMPLES);
  call_audio_level_add(&s_w.rx_level, pcm, CALL_AUDIO_PCM_SAMPLES);
  call_audio_es_far(&s_work->es, pcm, CALL_AUDIO_PCM_SAMPLES);
  if (!s_stats.speaker_on) {
    return;
  }
  switch (call_audio_rx_decide(speaker_service_stream_buffered_owned(PebbleTask_Unknown), gap)) {
    case CALL_AUDIO_RX_DROP:
      s_stats.rx_drops++;
      return;
    case CALL_AUDIO_RX_FILL_THEN_WRITE:
      s_stats.rx_fills++;
      prv_speaker_write(s_silence);
      break;
    case CALL_AUDIO_RX_WRITE:
      break;
  }
  if (pcm != decoded) {
    memcpy(decoded, pcm, sizeof(decoded));
  }
  call_audio_gain(decoded, CALL_AUDIO_PCM_SAMPLES, s_rx_shift);
  call_audio_up2(&s_work->up, decoded, CALL_AUDIO_PCM_SAMPLES, s_work->spk);
  prv_speaker_write(s_work->spk);
}

static void prv_levels(void) {
  const uint64_t now = pbl_uptime_ms();
  if (now - s_w.level_ms < LEVEL_PERIOD_MS) {
    return;
  }
  s_w.level_ms = now;
  call_audio_level_take(&s_w.rx_level, &s_stats.rx_rms, &s_stats.rx_mean);
  call_audio_level_take(&s_w.tx_level, &s_stats.tx_rms, &s_stats.tx_mean);
  call_audio_steps_take(&s_w.rx_steps, &s_stats.rx_edge_step, &s_stats.rx_inner_step);
}

static void prv_check_stack(void) {
  for (int i = 0; i < STACK_GUARD_WORDS; i++) {
    if (s_stack[i] != STACK_GUARD_WORD) {
      pbl_panic("CallAudio stack overflow");
    }
  }
}

static void prv_main(void *arg) {
  uint32_t pkt[CALL_AUDIO_RX_BYTES / 4];
  while (!s_quit) {
    prv_check_stack();
    (void)pbl_sem_take(&s_wake, PBL_MSEC(20));
    if (s_stats.mic_on) {
      mic_poll(MIC);
    }
    size_t len;
    while ((len = lcpu_audio_rx_packet((uint8_t *)pkt, sizeof(pkt))) != 0) {
      prv_rx_packet((const uint8_t *)pkt, len);
    }
    prv_levels();
  }
}

/* Start and stop */

static void prv_release(void) {
  kernel_free(s_stack);
  kernel_free(s_mic_buf);
  kernel_free(s_work);
  s_stack = NULL;
  s_mic_buf = NULL;
  s_work = NULL;
}

void call_audio_start(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_stats.running) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  s_stack = kernel_malloc(STACK_GUARD_WORDS * sizeof(uint32_t) + STACK_SIZE);
  s_mic_buf = kernel_malloc(MIC_FRAME_SAMPLES * sizeof(int16_t));
  s_work = kernel_zalloc(sizeof(*s_work));
  if (s_stack == NULL || s_mic_buf == NULL || s_work == NULL) {
    prv_release();
    pbl_mutex_unlock(&s_lock);
    return;
  }
  for (int i = 0; i < STACK_GUARD_WORDS; i++) {
    s_stack[i] = STACK_GUARD_WORD;
  }

  const uint16_t starts = s_stats.starts;
  s_stats = (struct call_audio_stats){.starts = starts + 1, .stack_size = STACK_SIZE};
  s_w = (typeof(s_w)){.level_ms = pbl_uptime_ms()};
  call_audio_cvsd_init(&s_work->cvsd);
  call_audio_agc_init(&s_work->agc);
  call_audio_es_init(&s_work->es);
  s_quit = false;
  while (pbl_sem_take(&s_wake, PBL_NO_WAIT) == 0) {
  }

  s_stats.held = lcpu_audio_hold(true);
  // What the LCPU queued while nobody read is old by now.
  prv_flush_rx();
  s_stats.speaker_on = speaker_service_stream_open_realtime_owned(
      SpeakerPriorityCritical, s_volume, SpeakerPcmFormat_16kHz_16bit, PebbleTask_Unknown);
  for (int i = 0; s_stats.speaker_on && i < PREFILL_PACKETS; i++) {
    prv_speaker_write(s_silence);
  }
  mic_set_volume((MicDevice *)MIC, MIC_CALL_GAIN);
  s_stats.mic_on =
      mic_start_polling(MIC, prv_mic_frame, NULL, s_mic_buf, MIC_FRAME_SAMPLES, prv_wake);
  lcpu_audio_set_notify(prv_wake, NULL);

  const struct pbl_thread_attr attr = {
    .name = "CallAudio",
    .entry = prv_main,
    .prio = PBL_PRIO_IDLE + 3,
    .privileged = true,
    .stack = &s_stack[STACK_GUARD_WORDS],
    .stack_size = STACK_SIZE,
  };
  s_stats.running = pbl_thread_create(&s_thread, &attr) == 0;
  if (!s_stats.running) {
    lcpu_audio_set_notify(NULL, NULL);
    if (s_stats.mic_on) {
      mic_stop(MIC);
      mic_set_volume((MicDevice *)MIC, MIC_DEFAULT_GAIN);
    }
    if (s_stats.speaker_on) {
      speaker_service_stream_close_owned(PebbleTask_Unknown);
    }
    s_stats = (struct call_audio_stats){.starts = s_stats.starts};
    (void)lcpu_audio_hold(false);
    prv_release();
  }
  pbl_mutex_unlock(&s_lock);
}

// The call's audio ends mid-word: from the last sample written down to 0 over a frame, then
// some silence, so the speaker does not stop on a step (a click).
#define FADE_SILENCE_FRAMES 4

static void prv_speaker_fade_out(void) {
  if (s_work == NULL) {
    return;
  }
  int16_t *pcm = s_work->spk;
  const int32_t last = pcm[CALL_AUDIO_SPK_SAMPLES - 1];
  for (int i = 0; i < CALL_AUDIO_SPK_SAMPLES; i++) {
    pcm[i] = (int16_t)(last * (CALL_AUDIO_SPK_SAMPLES - 1 - i) / CALL_AUDIO_SPK_SAMPLES);
  }
  prv_speaker_write(pcm);
  for (int i = 0; i < FADE_SILENCE_FRAMES; i++) {
    prv_speaker_write(s_silence);
  }
}

void call_audio_stop(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_stats.running) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  lcpu_audio_set_notify(NULL, NULL);
  s_quit = true;
  pbl_sem_give(&s_wake);
  for (int t = 0; t < 100 && pbl_thread_state(&s_thread) != PBL_THREAD_DEAD; t++) {
    pbl_thread_sleep(PBL_MSEC(5));
  }
  if (pbl_thread_state(&s_thread) != PBL_THREAD_DEAD) {
    pbl_thread_abort(&s_thread);
  }
  prv_check_stack();
  struct pbl_thread_stack_info info;
  pbl_thread_stack_info(&s_thread, &info);
  s_stats.stack_used = (uint16_t)(info.size - info.high_water);

  if (s_stats.mic_on) {
    mic_stop(MIC);
  }
  mic_set_volume((MicDevice *)MIC, MIC_DEFAULT_GAIN);
  if (s_stats.speaker_on) {
    prv_speaker_fade_out();
    speaker_service_stream_close_owned(PebbleTask_Unknown);
  }
  s_stats.held = lcpu_audio_hold(false);
  s_stats.running = false;
  s_stats.speaker_on = false;
  s_stats.mic_on = false;
  prv_release();
  pbl_mutex_unlock(&s_lock);
}

static uint8_t prv_volume_step(int direction, bool wrap) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (direction > 0) {
    // Up to 100, then the gain on top, then (wrapping) around to the lowest.
    if (s_volume < 100) {
      s_volume = (uint8_t)(s_volume + VOLUME_STEP > 100 ? 100 : s_volume + VOLUME_STEP);
    } else if (s_rx_shift < RX_SHIFT_MAX) {
      s_rx_shift++;
    } else if (wrap) {
      s_volume = VOLUME_LOWEST;
      s_rx_shift = 0;
    }
  } else if (direction < 0) {
    if (s_rx_shift > 0) {
      s_rx_shift--;
    } else if (s_volume > VOLUME_LOWEST) {
      s_volume = (uint8_t)(s_volume - VOLUME_STEP);
    }
  }
  if (direction != 0 && s_stats.speaker_on) {
    speaker_service_set_volume_owned(PebbleTask_Unknown, s_volume);
  }
  const uint8_t vol = s_volume;
  pbl_mutex_unlock(&s_lock);
  return vol;
}

uint8_t call_audio_volume_step(int direction) {
  return prv_volume_step(direction, true);
}

uint8_t call_audio_volume_change(int direction) {
  return prv_volume_step(direction, false);
}

uint8_t call_audio_volume_percent(void) {
  // The steps call_audio_volume_change() goes through: 30 to 100, then the gains on top.
  const unsigned steps = (100 - VOLUME_LOWEST) / VOLUME_STEP + 1 + RX_SHIFT_MAX;
  const unsigned step = (s_volume - VOLUME_LOWEST) / VOLUME_STEP + s_rx_shift;
  return (uint8_t)((step + 1) * 100 / steps);
}

uint8_t call_audio_rx_gain(void) {
  return (uint8_t)(1U << s_rx_shift);
}

int call_audio_mic_gain_step(int direction) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (direction != 0) {
    // The automatic gain, then 1, 2, 4, 8, and around.
    s_mic_shift = s_mic_shift == MIC_SHIFT_AGC ? 0 : s_mic_shift + 1;
  }
  int gain;
  if (s_mic_shift != MIC_SHIFT_AGC) {
    gain = 1 << s_mic_shift;
  } else if (s_work != NULL) {
    gain = CALL_AUDIO_MIC_AGC + call_audio_agc_gain_db10(&s_work->agc);
  } else {
    gain = CALL_AUDIO_MIC_AGC;
  }
  pbl_mutex_unlock(&s_lock);
  return gain;
}

void call_audio_get_stats(struct call_audio_stats *out) {
  *out = s_stats;
}
