/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

#include <pbl/drivers/audio.h>

/**
 * @defgroup drivers_audio_sim Simulator
 * @ingroup drivers_audio
 * @brief Simulated microphone, recording the audio written to it.
 *
 * Records 16 kHz mono audio, paced by a timer: what pbl_audio_sim_mic_write() queues, and silence
 * when nothing is queued.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_audio pbl_audio_sim_mic;
/** @endcond */

/** @brief The simulated microphone, as a capture device. */
#define SIM_MIC (&pbl_audio_sim_mic)

/**
 * @brief Queue audio for the microphone to record.
 *
 * Drops the oldest queued audio once the queue is full. Ignored while it is not recording.
 *
 * @param mic Device.
 * @param pcm 16 kHz mono signed 16-bit PCM, in CPU byte order. Need not be aligned.
 * @param count Number of samples.
 */
void pbl_audio_sim_mic_write(const struct pbl_audio *mic, const void *pcm, size_t count);

/** @} */
