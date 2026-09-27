/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_PLAYBACK_SAMPLE_RATE (16000) //!< PCM and clock rate seen by AudioPlaybackCB

typedef const struct AudioDevice AudioDevice;
//! Invoked on the system task; consumers may refill synchronously.
typedef void (*AudioTransCB)(uint32_t *free_size);

//! Optional observer of mono PCM committed to DMA, including underrun silence. sample_time is
//! its presentation time on a wrapping AUDIO_PLAYBACK_SAMPLE_RATE clock derived from uptime and
//! re-anchored whenever it drifts more than 8 ms from it.
//! Runs in the DMA ISR; copy immediately and never block or retain the sample pointer.
typedef void (*AudioPlaybackCB)(const int16_t *samples, size_t count, uint32_t sample_time,
                                void *context);
//! Provided by drivers that select SPEAKER_PLAYBACK_OBSERVER. NULL unregisters synchronously.
//! Returns false when the device format cannot be observed.
bool audio_set_playback_callback(AudioDevice *audio_device, AudioPlaybackCB cb, void *context);

//! Optional board-level power hooks. Either callback may be NULL.
//! power_up runs before the consumer enables (e.g. raise PMIC discharge limit);
//! power_down runs after the consumer disables (e.g. restore the limit).
typedef struct BoardPowerOps {
  void (*power_up)(void);
  void (*power_down)(void);
} BoardPowerOps;

extern void audio_init(AudioDevice *audio_device);
extern void audio_start(AudioDevice *audio_device, AudioTransCB cb);
//! Returns remaining queue space in bytes; a zero-size write only queries space.
extern uint32_t audio_write(AudioDevice *audio_device, void *writeBuf, uint32_t size);
// audio volume from 0~100
extern void audio_set_volume(AudioDevice *audio_device, int volume);
extern void audio_stop(AudioDevice *audio_device);
