/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Pure parts of the call audio: packet format, rate conversion, level, and how
// to keep the speaker's queue in step with the phone's packets.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A received packet: a 4-byte header (length, packet status, reserved) and 7.5 ms of audio:
// the CVSD air bits (60 bytes, with the LCPU set to hand them over), or 8 kHz 16-bit PCM
// (120 bytes) where the LCPU converted them.
#define CALL_AUDIO_PCM_BYTES    120
#define CALL_AUDIO_AIR_BYTES    60
#define CALL_AUDIO_RX_HDR_BYTES 4
#define CALL_AUDIO_RX_BYTES     (CALL_AUDIO_RX_HDR_BYTES + CALL_AUDIO_PCM_BYTES)
#define CALL_AUDIO_PCM_SAMPLES  (CALL_AUDIO_PCM_BYTES / 2)
//! A packet's 7.5 ms doubled to 16 kHz for the speaker.
#define CALL_AUDIO_SPK_SAMPLES (2 * CALL_AUDIO_PCM_SAMPLES)
#define CALL_AUDIO_SPK_BYTES   (2 * CALL_AUDIO_PCM_BYTES)

//! Whether @p pkt (@p len bytes) is a whole received packet: its header gives the length of
//! what follows, air bits or PCM, and a packet status of 0-3.
bool call_audio_rx_header_ok(const uint8_t *pkt, size_t len);

//! Rate halving and doubling between 16 kHz and 8 kHz through a 31-tap half-band FIR (Kaiser,
//! beta 5.5): flat to 3 kHz, 0.5 dB down at 3.4 kHz, 24 dB at 4.6 kHz, 70 dB from 6 kHz.
#define CALL_AUDIO_HALFBAND_TAPS 31
struct call_audio_halfband {
  //! The input's last samples (zero-stuffed when doubling), carried between calls.
  int16_t hist[CALL_AUDIO_HALFBAND_TAPS - 1];
};

//! @p count (even) 16 kHz samples to @p count / 2 at 8 kHz.
void call_audio_down2(struct call_audio_halfband *f, const int16_t *in, size_t count, int16_t *out);
//! @p count 8 kHz samples to 2 * @p count at 16 kHz.
void call_audio_up2(struct call_audio_halfband *f, const int16_t *in, size_t count, int16_t *out);

//! Running level of the audio.
struct call_audio_level {
  uint64_t sum_sq;
  int64_t sum;
  uint32_t count;
};

void call_audio_level_add(struct call_audio_level *l, const int16_t *samples, size_t count);

//! Automatic gain for the microphone, per call of call_audio_agc_run() (a frame):
//! - a high-pass at about 135 Hz first (the arm's movements, rumble, the mic's offset);
//! - the frame's RMS against a running noise floor (the first frame's but no more than
//!   CALL_AUDIO_AGC_FLOOR_FIRST, then down at once and up about 2.3 dB a second, never below
//!   CALL_AUDIO_AGC_FLOOR_MIN): speech is at least
//!   CALL_AUDIO_AGC_SPEECH_X times it, and CALL_AUDIO_AGC_GATE;
//! - on speech, and unless @c hold, the gain goes to what brings the RMS to
//!   CALL_AUDIO_AGC_TARGET (about -16 dBFS; the CVSD codec distorts louder high tones), between
//!   CALL_AUDIO_AGC_MIN_Q8 and CALL_AUDIO_AGC_MAX_Q8 (/ 256), at most 0.5 dB a frame up and 3 dB
//!   down; otherwise it is
//!   held, so the background noise is not brought up between words;
//! - whatever the frame, the gain goes down at once to keep its peak below CALL_AUDIO_AGC_PEAK
//!   (but not below CALL_AUDIO_AGC_MIN_Q8); on @c hold for that frame only, and the noise floor
//!   is not followed then either;
//! - CALL_AUDIO_AGC_HANG frames after the last speech, the output goes down 1.5 dB a frame to
//!   CALL_AUDIO_AGC_QUIET_Q15 / 32768 (-12 dB), and back at once on speech.
//! Gain changes are ramped over the frame. Q8 gain.
#define CALL_AUDIO_AGC_TARGET      5000
#define CALL_AUDIO_AGC_PEAK        18000
#define CALL_AUDIO_AGC_GATE        30
#define CALL_AUDIO_AGC_MIN_Q8      (256 / 4)
#define CALL_AUDIO_AGC_MAX_Q8      (64 * 256)
#define CALL_AUDIO_AGC_FLOOR_FIRST 150
#define CALL_AUDIO_AGC_FLOOR_MIN   16
#define CALL_AUDIO_AGC_SPEECH_X    3
#define CALL_AUDIO_AGC_HANG        12
#define CALL_AUDIO_AGC_QUIET_Q15   8192
struct call_audio_agc {
  uint32_t gain_q8;
  //! The gain does not move (the far end talks: what the mic hears is mostly its echo).
  bool hold;
  //! The noise floor's RMS, Q8.
  uint32_t floor_q8;
  //! The output's level between words, Q15, and frames since the last speech.
  int32_t quiet_q15;
  uint32_t since_speech;
  //! The total gain the last frame ended on (Q8 times Q15, as Q8), for the ramp.
  uint32_t last_q8;
  //! The high-pass's last input and output.
  int32_t hp_x;
  int32_t hp_y;
};
void call_audio_agc_init(struct call_audio_agc *a);
void call_audio_agc_run(struct call_audio_agc *a, int16_t *samples, size_t count);
//! The gain in tenths of a dB.
int call_audio_agc_gain_db10(const struct call_audio_agc *a);

//! Echo suppression for the speaker call: while the far end talks (its envelope above
//! CALL_AUDIO_ES_FAR_ON, and CALL_AUDIO_ES_HANG samples after), the microphone goes down to
//! CALL_AUDIO_ES_DUCK_Q15 / 32768 (-18 dB), in about 5 ms, and back in about 40 ms, so the
//! watch's speaker does not come back to the far end as an echo. Both sides at 8 kHz.
#define CALL_AUDIO_ES_FAR_ON   500
#define CALL_AUDIO_ES_HANG     2400
#define CALL_AUDIO_ES_DUCK_Q15 4096
struct call_audio_es {
  int32_t far_env;
  uint32_t hang;
  int32_t gain_q15;
};

void call_audio_es_init(struct call_audio_es *es);
//! The far end's samples (decoded, before the speaker's gain).
void call_audio_es_far(struct call_audio_es *es, const int16_t *samples, size_t count);
//! The microphone's samples, ducked while the far end talks.
void call_audio_es_near(struct call_audio_es *es, int16_t *samples, size_t count);
//! Whether the far end talks (or did, within CALL_AUDIO_ES_HANG).
bool call_audio_es_far_active(const struct call_audio_es *es);

//! Multiply @p count samples by 1 << @p shift, saturating.
void call_audio_gain(int16_t *samples, size_t count, unsigned int shift);

//! Mean absolute steps between samples, at the edges between packets (the last sample of one
//! and the first of the next) and inside them: a conversion that restarts each packet shows as
//! edge steps well above the inner ones.
struct call_audio_steps {
  uint64_t edge_sum;
  uint64_t inner_sum;
  uint32_t edges;
  uint32_t inners;
  int16_t last;
  bool have_last;
};

void call_audio_steps_add(struct call_audio_steps *s, const int16_t *samples, size_t count);
//! The means since the last take, then start over (the packet edge is kept).
void call_audio_steps_take(struct call_audio_steps *s, uint16_t *edge, uint16_t *inner);
//! Root mean square and mean of what was added, then start over.
void call_audio_level_take(struct call_audio_level *l, uint16_t *rms, int16_t *mean);

enum call_audio_rx_action {
  CALL_AUDIO_RX_WRITE,
  //! The speaker's queue has too much: drop the packet so the delay does not grow.
  CALL_AUDIO_RX_DROP,
  //! The speaker ran dry (a gap in the packets): put one packet of silence before it.
  CALL_AUDIO_RX_FILL_THEN_WRITE,
};

//! Bytes in the speaker's queue (16 kHz PCM) above which a packet is dropped (60 ms).
#define CALL_AUDIO_RX_MAX_BUFFERED (8 * CALL_AUDIO_SPK_BYTES)
//! Gap between packets, in ms, after which the speaker is taken to have run dry.
#define CALL_AUDIO_RX_GAP_MS 20

enum call_audio_rx_action call_audio_rx_decide(uint32_t buffered_bytes, uint32_t gap_ms);
