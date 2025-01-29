/*
 * Copyright 2024 Google LLC
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

//! This function extracts a value for a specific bit per pixel depth from an image buffer
//! at a specific x y position.
//! @note inlined to support performance requirements of iterating over every pixel in an image
//! @param buffer pointer to a buffer containing pixel image data
//! @param x the x coordinates for the pixel to retrieve
//! @param y the y coordinates for the pixel to retrieve
//! @param width provides the image width
//! @param row_stride_bytes the byte-aligned width in bytes
//! @param bitdepth bits per pixel for the image (1,2,4 or 8 supported)
//! @return The value from the image buffer at the specified coordinates
static ALWAYS_INLINE uint8_t raw_image_get_value_for_bitdepth(const uint8_t *raw_image_buffer,
    uint32_t x, uint32_t y, uint16_t row_stride_bytes, uint8_t bitdepth) {
  // Retrieve the byte from the image buffer containing the requested pixel
  uint32_t pixel_in_byte = raw_image_buffer[y * row_stride_bytes + (x * bitdepth / 8)];
  // Find the index of the pixel in terms of coordinates and aligned_width
  uint32_t pixel_index =  y * (row_stride_bytes * 8 / bitdepth) + x;
  // Shift and mask the requested pixel data from the byte containing it and return
  return (uint8_t)((pixel_in_byte >> ((((8 / bitdepth) - 1) - (pixel_index % (8 / bitdepth)))
          * bitdepth)) & ~(~0U << bitdepth));
}

//! This function sets a pixel value for a specific bits-per-pixel depth in an image buffer
//! at a specific (x, y) coordinate.
//! @note inlined to support performance requirements of iterating over every pixel in an image
//! @param raw_image_buffer Pointer to a buffer containing image pixel data
//! @param x The x coordinate for the pixel to retrieve
//! @param y The y coordinate for the pixel to retrieve
//! @param row_stride_bytes The byte-aligned width of each row in bytes
//! @param bitdepth The bits-per-pixel for the image (Only 1, 2, 4 or 8 bitdepths are supported)
//! @param value The pixel value to set in the image buffer at the specified (x, y) coordinates
static ALWAYS_INLINE void raw_image_set_value_for_bitdepth(uint8_t *raw_image_buffer,
                                                           uint32_t x, uint32_t y,
                                                           uint16_t row_stride_bytes,
                                                           uint8_t bitdepth, uint8_t value) {
  const uint8_t pixels_per_byte = (uint8_t)(8 / bitdepth);

  // Retrieve the byte from the image buffer containing the requested pixel
  const uint32_t byte_offset = y * row_stride_bytes + (x * bitdepth / 8);
  const uint8_t pixel_in_byte = raw_image_buffer[byte_offset];

  // Find the index of the pixel in terms of coordinates and aligned_width
  const uint32_t pixel_index =  (y * (row_stride_bytes * pixels_per_byte) + x) % pixels_per_byte;

  // For example, bitdepth=1 -> bitdepth_mask=0b1, bitdepth=2 -> bitdepth_mask=0b11, etc.
  const uint8_t bitdepth_mask = (uint8_t)~(~0U << bitdepth);

  const uint32_t bits_to_shift = (pixels_per_byte - 1 - pixel_index) * bitdepth;

  const uint8_t value_position_mask = ~(bitdepth_mask << bits_to_shift);

  const uint8_t value_shifted_to_position = (value & bitdepth_mask) << bits_to_shift;

  // Finally, update the byte where the pixel is located using the value_mask
  const uint8_t new_byte_value = (pixel_in_byte & value_position_mask) | value_shifted_to_position;
  raw_image_buffer[byte_offset] = new_byte_value;
}
