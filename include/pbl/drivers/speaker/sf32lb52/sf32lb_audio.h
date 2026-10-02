/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @addtogroup drivers_speaker_sf32lb52
 * @{
 */

/**
 * @brief Initialize the AUDCODEC DAC path.
 *
 * @param audio_device Audio device.
 * @return True on success.
 */
extern bool audec_init(AudioDevice *audio_device);

/**
 * @brief Start DAC playback, backing audio_start().
 *
 * @param audio_device Audio device.
 * @param cb Called on the system task whenever the device wants more data.
 */
extern void audec_start(AudioDevice *audio_device, AudioTransCB cb);

/**
 * @brief Queue PCM data, backing audio_write().
 *
 * @param audio_device Audio device.
 * @param writeBuf 16-bit PCM samples.
 * @param size Size of @p writeBuf in bytes; data that does not fit is dropped.
 * @return Free queue space in bytes after the write, 0 if not started.
 */
extern uint32_t audec_write(AudioDevice *audio_device, void *writeBuf, uint32_t size);

/**
 * @brief Set the DAC volume, backing audio_set_volume().
 *
 * @param audio_device Audio device.
 * @param volume Volume from 0 to 100, clamped.
 */
extern void audec_set_vol(AudioDevice *audio_device, int volume);

/**
 * @brief Stop DAC playback, backing audio_stop().
 *
 * @param audio_device Audio device.
 */
extern void audec_stop(AudioDevice *audio_device);

/** @} */