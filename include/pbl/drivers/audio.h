/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef const struct AudioDevice AudioDevice;
typedef void (*AudioTransCB)(uint32_t *free_size);

//! Optional board-level power hooks. Either callback may be NULL.
//! power_up runs before the consumer enables (e.g. raise PMIC discharge limit);
//! power_down runs after the consumer disables (e.g. restore the limit).
typedef struct BoardPowerOps {
  void (*power_up)(void);
  void (*power_down)(void);
} BoardPowerOps;

extern void audio_init(AudioDevice* audio_device);
extern void audio_start(AudioDevice* audio_device, AudioTransCB cb);
extern uint32_t audio_write(AudioDevice* audio_device, void *writeBuf, uint32_t size);
//audio volume from 0~100
extern void audio_set_volume(AudioDevice* audio_device, int volume);
extern void audio_stop(AudioDevice* audio_device);