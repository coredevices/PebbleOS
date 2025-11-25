/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

typedef const struct AudioDevice AudioDevice;
typedef void (*AudioTransCB)(uint32_t *free_size);

extern void audio_init(AudioDevice* audio_device);
extern void audio_start(AudioDevice* audio_device, AudioTransCB cb);
extern uint32_t audio_write(AudioDevice* audio_device, void *writeBuf, uint32_t size);
//audio volume from 0~100
extern void audio_set_volume(AudioDevice* audio_device, int volume);
extern void audio_stop(AudioDevice* audio_device);