/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */


#pragma once
 
extern bool audec_init(AudioDevice* audio_device);
extern void audec_start(AudioDevice* audio_device, AudioTransCB cb);
extern uint32_t audec_write(AudioDevice* audio_device, void *writeBuf, uint32_t size);
extern void audec_set_vol(AudioDevice* audio_device, int volume);
extern void audec_stop(AudioDevice* audio_device);