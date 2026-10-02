/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup drivers_audio Audio playback
 * @ingroup drivers
 * @brief Speaker (audio output) driver interface.
 *
 * The driver plays 16-bit mono PCM queued with audio_write(). Once started, it asks for more
 * data through the AudioTransCB passed to audio_start() whenever queue space frees up. The
 * per-implementation @c AudioDevice definitions live in the speaker driver groups.
 *
 * @code{.c}
 * static void prv_refill(uint32_t *free_size) {
 *   size_t len = MIN(*free_size, sizeof(s_pcm));
 *   generate_pcm(s_pcm, len);
 *   audio_write(AUDIO, s_pcm, len);
 * }
 *
 * audio_init(AUDIO);
 * audio_set_volume(AUDIO, 80);
 * audio_start(AUDIO, prv_refill);
 * ...
 * audio_stop(AUDIO);
 * @endcode
 * @{
 */

/** @brief PCM sample rate, in Hz, and rate of the clock seen by AudioPlaybackCB. */
#define AUDIO_PLAYBACK_SAMPLE_RATE (16000)

/** @brief Audio output device, defined by the driver implementation. */
typedef const struct AudioDevice AudioDevice;

/**
 * @brief Request for more audio data.
 *
 * Invoked on the system task; the consumer may call audio_write() synchronously from it.
 *
 * @param[in,out] free_size Free queue space in bytes.
 */
typedef void (*AudioTransCB)(uint32_t *free_size);

/**
 * @brief Observer of the mono PCM committed to DMA, including underrun silence.
 *
 * Runs in the DMA ISR: copy the samples immediately, never block or keep the pointer.
 *
 * @param samples Samples committed to DMA.
 * @param count Number of samples.
 * @param sample_time Presentation time of the first sample, on a wrapping
 *                    #AUDIO_PLAYBACK_SAMPLE_RATE clock derived from uptime and re-anchored
 *                    whenever it drifts more than 8 ms from it.
 * @param context User context given to audio_set_playback_callback().
 */
typedef void (*AudioPlaybackCB)(const int16_t *samples, size_t count, uint32_t sample_time,
                                void *context);

/**
 * @brief Register or unregister the playback observer.
 *
 * Provided by drivers that select @c SPEAKER_PLAYBACK_OBSERVER.
 *
 * @param audio_device Audio device.
 * @param cb Observer, or NULL to unregister synchronously.
 * @param context User context passed to @p cb.
 * @return False if the device format (sample rate, channels) cannot be observed.
 */
bool audio_set_playback_callback(AudioDevice *audio_device, AudioPlaybackCB cb, void *context);

/**
 * @brief Optional board-level power hooks.
 */
typedef struct BoardPowerOps {
  /** Run before playback starts, e.g. to raise the PMIC discharge limit. May be NULL. */
  void (*power_up)(void);
  /** Run after playback stops, e.g. to restore the limit. May be NULL. */
  void (*power_down)(void);
} BoardPowerOps;

/**
 * @brief Initialize the audio device.
 *
 * @param audio_device Audio device.
 */
extern void audio_init(AudioDevice *audio_device);

/**
 * @brief Start playback.
 *
 * @param audio_device Audio device.
 * @param cb Called whenever the device wants more data.
 */
extern void audio_start(AudioDevice *audio_device, AudioTransCB cb);

/**
 * @brief Queue PCM data for playback.
 *
 * Data that does not fit in the queue is dropped.
 *
 * @param audio_device Audio device.
 * @param writeBuf 16-bit PCM samples.
 * @param size Size of @p writeBuf in bytes; 0 only queries the free space.
 * @return Free queue space in bytes after the write.
 */
extern uint32_t audio_write(AudioDevice *audio_device, void *writeBuf, uint32_t size);

/**
 * @brief Set the playback volume.
 *
 * @param audio_device Audio device.
 * @param volume Volume from 0 to 100.
 */
extern void audio_set_volume(AudioDevice *audio_device, int volume);

/**
 * @brief Stop playback.
 *
 * @param audio_device Audio device.
 */
extern void audio_stop(AudioDevice *audio_device);

/** @} */
