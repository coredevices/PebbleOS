/*
 * Copyright 2025 Joshua Jun
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "services/normal/voice_endpoint.h"

/**
 * @brief Initialize the Speex encoder
 * @return true if successful, false otherwise
 */
bool voice_speex_init(void);

/**
 * @brief Deinitialize the Speex encoder
 */
void voice_speex_deinit(void);

/**
 * @brief Get transfer info for audio endpoint
 * @param info Pointer to AudioTransferInfoSpeex structure to fill
 */
void voice_speex_get_transfer_info(AudioTransferInfoSpeex *info);

/**
 * @brief Get the frame size in samples
 * @return frame size in samples, or 0 if not initialized
 */
int voice_speex_get_frame_size(void);

/**
 * @brief Get the frame buffer for audio input
 * @return pointer to frame buffer, or NULL if not initialized
 */
int16_t *voice_speex_get_frame_buffer(void);

/**
 * @brief Get the frame buffer size in bytes
 * @return frame buffer size in bytes, or 0 if not initialized
 */
size_t voice_speex_get_frame_buffer_size(void);

/**
 * @brief Encode a frame of audio samples
 * @param samples Pointer to 16-bit audio samples (processed in-place)
 * @param encoded_data Buffer to store encoded data
 * @param max_encoded_size Maximum size of encoded data buffer
 * @return number of encoded bytes, or -1 on error
 */
int voice_speex_encode_frame(int16_t *samples, uint8_t *encoded_data, size_t max_encoded_size);

/**
 * @brief Check if Speex encoder is initialized
 * @return true if initialized, false otherwise
 */
bool voice_speex_is_initialized(void);
