/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup drivers_mic Microphone
 * @ingroup drivers
 * @brief Microphone driver interface.
 *
 * The driver captures 16-bit PCM at #MIC_SAMPLE_RATE into a caller-provided buffer and hands
 * each full buffer to a data handler. The per-implementation @c MicDevice definitions live in
 * the microphone driver subgroups.
 *
 * @code{.c}
 * static int16_t s_frame[320];
 *
 * static void prv_frame(int16_t *samples, size_t sample_count, void *context) {
 *   encode(samples, sample_count);
 * }
 *
 * mic_init(MIC);
 * mic_start(MIC, prv_frame, NULL, s_frame, ARRAY_LENGTH(s_frame));
 * ...
 * mic_stop(MIC);
 * @endcode
 * @{
 */

/** @brief Sample rate of the captured audio, in Hz. */
#define MIC_SAMPLE_RATE (16000)
/** @brief Volume value meaning the driver default; not used by the drivers. */
#define MIC_DEFAULT_VOLUME (-1)

/** @brief Microphone device, defined by the driver implementation. */
typedef const struct MicDevice MicDevice;

/**
 * @brief Handler for a full buffer of captured audio.
 *
 * Runs on the system task, or on the consumer's task from mic_poll() in polling mode.
 *
 * @param samples Captured samples, the buffer passed when starting.
 * @param sample_count Number of 16-bit samples in @p samples.
 * @param context User context passed when starting.
 */
typedef void (*MicDataHandlerCB)(int16_t *samples, size_t sample_count, void *context);

/**
 * @brief Initialize the microphone driver.
 *
 * Called once at boot.
 *
 * @param this Microphone device.
 */
void mic_init(MicDevice *this);

/**
 * @brief Set the capture gain.
 *
 * Must be called after mic_init() and while the microphone is stopped; ignored while running.
 *
 * @param this Microphone device.
 * @param volume Gain; the range is driver specific (0-100 on SF32LB52, 0-1024 on nRF5).
 */
void mic_set_volume(MicDevice *this, uint16_t volume);

/**
 * @brief Start capturing.
 *
 * @p data_handler is called each time @p audio_buffer has been filled.
 *
 * @param this Microphone device.
 * @param data_handler Handler for each full buffer.
 * @param context User context passed to @p data_handler.
 * @param audio_buffer Buffer the driver fills, owned by the caller until mic_stop() returns.
 * @param audio_buffer_len Capacity of @p audio_buffer, in 16-bit samples.
 * @return True if capture started, false if it was already running or could not start.
 */
bool mic_start(MicDevice *this, MicDataHandlerCB data_handler, void *context, int16_t *audio_buffer,
               size_t audio_buffer_len);

/**
 * @brief Notification that a frame is ready for mic_poll().
 *
 * Runs from the DMA ISR; it must only wake the consumer.
 *
 * @param context User context passed to mic_start_polling().
 */
typedef void (*MicDataReadyCB)(void *context);

/**
 * @brief Start capturing with realtime dispatch.
 *
 * Provided by drivers that select @c MIC_POLLING. Instead of calling @p data_handler on the
 * system task, the driver calls @p ready and the consumer calls mic_poll() from its own task to
 * receive the frames.
 *
 * @param this Microphone device.
 * @param data_handler Handler for each full buffer, called from mic_poll().
 * @param context User context passed to @p data_handler and @p ready.
 * @param audio_buffer Buffer the driver fills, owned by the caller until mic_stop() returns.
 * @param audio_buffer_len Capacity of @p audio_buffer, in 16-bit samples.
 * @param ready Called when frames are ready.
 * @return False, without starting capture, if the device is busy.
 */
bool mic_start_polling(MicDevice *this, MicDataHandlerCB data_handler, void *context,
                       int16_t *audio_buffer, size_t audio_buffer_len, MicDataReadyCB ready);

/**
 * @brief Deliver the pending frames to the data handler.
 *
 * Only for capture started with mic_start_polling().
 *
 * @param this Microphone device.
 */
void mic_poll(MicDevice *this);

/**
 * @brief Get the capture time of the frame being delivered.
 *
 * Only valid inside the data handler.
 *
 * @param this Microphone device.
 * @param[out] sample_time Time of the first sample of the frame, on a wrapping #MIC_SAMPLE_RATE
 *                         clock derived from uptime and re-anchored whenever it drifts more than
 *                         8 ms from it.
 * @return True if @p sample_time was set, false outside the data handler.
 */
bool mic_get_frame_time(MicDevice *this, uint32_t *sample_time);

/**
 * @brief Stop capturing.
 *
 * Samples in a partially filled buffer are dropped. Once this returns, no more handlers run and
 * the buffer is no longer written.
 *
 * @param this Microphone device.
 */
void mic_stop(MicDevice *this);

/**
 * @brief Check whether the microphone is running.
 *
 * @param this Microphone device.
 * @return True if capturing.
 */
bool mic_is_running(MicDevice *this);

/**
 * @brief Get the number of audio channels.
 *
 * @param this Microphone device.
 * @return 1 for mono, 2 for stereo; 1 if the device does not specify it.
 */
uint32_t mic_get_channels(MicDevice *this);

/** @} */
