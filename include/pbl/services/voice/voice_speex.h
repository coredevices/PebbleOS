/* SPDX-FileCopyrightText: 2025 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "pbl/services/voice_endpoint.h"

/**
 * @defgroup services_voice_voice_speex Speex encoder
 * @ingroup services_voice
 * @brief Speex wideband (16 kHz) encoder used to stream dictation audio.
 *
 * The encoder owns the microphone frame buffer. Samples are boosted by a fixed gain before
 * encoding; stereo input is downmixed to mono with Speex stereo side information.
 * @{
 */

/**
 * @brief Initialize the encoder and allocate its buffers.
 *
 * Does nothing if already initialized.
 *
 * @return true on success.
 */
bool voice_speex_init(void);

/** @brief Release the encoder and its buffers. */
void voice_speex_deinit(void);

/**
 * @brief Get the transfer info sent to the phone before the encoded audio.
 *
 * The encoder must be initialized.
 *
 * @param[out] info Transfer info.
 */
void voice_speex_get_transfer_info(AudioTransferInfoSpeex *info);

/**
 * @brief Get the frame size.
 *
 * @return Samples per frame across all channels, or 0 if not initialized.
 */
int voice_speex_get_frame_size(void);

/**
 * @brief Get the buffer the microphone fills with one frame.
 *
 * @return Frame buffer, or NULL if not initialized.
 */
int16_t *voice_speex_get_frame_buffer(void);

/**
 * @brief Get the frame buffer size.
 *
 * @return Frame buffer size in bytes, or 0 if not initialized.
 */
size_t voice_speex_get_frame_buffer_size(void);

/**
 * @brief Encode one frame.
 *
 * @param[in,out] samples voice_speex_get_frame_size() samples, modified in place (gain and
 * downmix).
 * @param[out] encoded_data Encoded output.
 * @param max_encoded_size Size of @p encoded_data in bytes.
 * @return Number of encoded bytes, or -1 on error.
 */
int voice_speex_encode_frame(int16_t *samples, uint8_t *encoded_data, size_t max_encoded_size);

/**
 * @brief Check whether the encoder is initialized.
 *
 * @return true if initialized.
 */
bool voice_speex_is_initialized(void);

/** @} */
