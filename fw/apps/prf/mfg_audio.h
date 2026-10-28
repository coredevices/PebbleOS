/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

//! Sample rate of the factory tests, in Hz.
#define MFG_AUDIO_SAMPLE_RATE 16000

//! Receives recorded interleaved samples on KernelBG. May stop the recording.
typedef void (*MfgAudioCaptureCb)(int16_t *samples, size_t count);

//! Fills up to @p count mono samples on KernelBG and returns how many it wrote. May stop the
//! playback.
typedef size_t (*MfgAudioFillCb)(int16_t *samples, size_t count);

//! Records the microphone; on boards with @c CONFIG_MIC. with @p channels channels at @p gain_mdb,
//! clamped to its range.
int mfg_audio_capture_start(uint8_t channels, int32_t gain_mdb, MfgAudioCaptureCb cb);

void mfg_audio_capture_stop(void);

//! Plays the speaker at @p gain_mdb, on boards with @c CONFIG_SPEAKER, raising the battery
//! discharge limit meanwhile.
int mfg_audio_playback_start(int32_t gain_mdb, MfgAudioFillCb cb);

void mfg_audio_playback_stop(void);
