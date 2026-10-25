/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @file
 * @brief A call's audio over the SF32LB52 LCPU's voice path.
 *
 * While a SCO link is up: the phone's audio (8 kHz PCM, the LCPU having
 * decoded the CVSD) goes from the LCPU's ring to the speaker, and the
 * microphone's (16 kHz, halved to 8 kHz) to the LCPU's ring towards the phone.
 * A thread does both, woken by the LCPU's voice notifications and the
 * microphone; its stack and buffers come from the kernel heap for the call.
 */

#include <stdbool.h>
#include <stdint.h>

struct call_audio_stats {
  bool running;
  bool speaker_on;
  bool mic_on;
  //! The LCPU is kept awake.
  bool held;
  uint16_t starts;

  //! Received packets, bad headers (the ring was then flushed), packets dropped because the
  //! speaker had too much, silences put in after a gap, short speaker writes.
  uint32_t rx_packets;
  uint32_t rx_bad;
  uint32_t rx_drops;
  uint32_t rx_fills;
  uint32_t rx_short;
  //! Header of the last packet, the longest gap between packets.
  uint8_t rx_hdr[4];
  uint16_t rx_max_gap_ms;
  //! Mean absolute step between the packets' edge samples, and between samples inside them,
  //! over the last second.
  uint16_t rx_edge_step;
  uint16_t rx_inner_step;
  //! Longest CVSD decode and encode of a packet, in microseconds (0: the LCPU converted).
  uint16_t rx_cvsd_us;
  uint16_t tx_cvsd_us;
  //! Level of the last second: root mean square and mean.
  uint16_t rx_rms;
  int16_t rx_mean;

  //! Packets put in the ring towards the phone, and packets that did not fit.
  uint32_t tx_packets;
  uint32_t tx_overflows;
  uint32_t mic_frames;
  uint16_t tx_rms;
  int16_t tx_mean;

  //! Stack of the audio thread at the end of the last call.
  uint16_t stack_used;
  uint16_t stack_size;
};

/** @brief Start the call's audio (a SCO link came up); nothing if already running. */
void call_audio_start(void);

/** @brief Stop it (the SCO link went down), releasing the speaker, microphone and memory. */
void call_audio_stop(void);

void call_audio_get_stats(struct call_audio_stats *out);

/**
 * @brief Speaker volume a step up (@p direction 1: by 10 to 100, then a gain of 2 and 4 on the
 *        phone's audio, then around to 30) or down (-1), or as it is (0); applies to the call
 *        playing and the next ones. @return The volume (0-100).
 */
uint8_t call_audio_volume_step(int direction);

//! Like call_audio_volume_step(), but stopping at the loudest instead of going around.
uint8_t call_audio_volume_change(int direction);

//! Where the volume is in call_audio_volume_change()'s steps, as a percentage (for a level bar).
uint8_t call_audio_volume_percent(void);

//! The gain on the phone's audio past volume 100 (1, 2, 4).
uint8_t call_audio_rx_gain(void);

/**
 * @brief The microphone's gain to the next setting (@p direction nonzero: automatic, then 1, 2,
 *        4, 8, and around) or as it is (0), on top of the microphone at full gain during a call.
 * @return The fixed gain (1-8), or for the automatic one, CALL_AUDIO_MIC_AGC + its gain now in
 *         tenths of a dB (-12 to +30 dB).
 */
#define CALL_AUDIO_MIC_AGC (-1000)
int call_audio_mic_gain_step(int direction);
