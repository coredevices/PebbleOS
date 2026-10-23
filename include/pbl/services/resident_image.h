/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/services/image_cache.h>

#include <applib/graphics/gtypes.h>

/**
 * @defgroup services_resident_image Resident images
 * @ingroup services
 * @brief Images kept in internal RAM, in small segments, for as long as they are shown.
 *
 * A large image does not need a contiguous block of the kernel heap: its rows are split into
 * segments of about @ref RESIDENT_IMAGE_SEGMENT_SIZE bytes, each its own allocation, so it fits a
 * fragmented heap. Each segment is drawn as a bitmap of its own.
 *
 * @code{.c}
 * ResidentImage *image = resident_image_create(GSize(144, 144), GBitmapFormat8Bit, NULL, 0);
 * resident_image_write(image, 0, pixels, length);
 * resident_image_draw(ctx, image, &GRect(0, 0, 144, 144));
 * resident_image_destroy(image);
 * @endcode
 * @{
 */

/** @brief Target size of a segment, in bytes. A segment always holds at least one row. */
#define RESIDENT_IMAGE_SEGMENT_SIZE (512)

/** @brief Largest palette of a resident image. */
#define RESIDENT_IMAGE_MAX_PALETTE (16)

/** @brief Resident image. */
typedef struct ResidentImage ResidentImage;

/**
 * @brief Create a resident image, with uninitialized pixels.
 *
 * @param size Size in pixels.
 * @param format Pixel format; @c GBitmapFormat8BitCircular is not supported.
 * @param palette Palette for palettized formats, copied, or NULL.
 * @param palette_count Number of entries in @p palette, at most @ref RESIDENT_IMAGE_MAX_PALETTE.
 * @return Image, or NULL if out of memory.
 */
ResidentImage *resident_image_create(GSize size, GBitmapFormat format, const GColor *palette,
                                     uint8_t palette_count);

/**
 * @brief Create a resident copy of a bitmap.
 *
 * @param bitmap Bitmap, whose rows start at its bounds origin.
 * @return Image, or NULL if out of memory.
 */
ResidentImage *resident_image_create_from_bitmap(const GBitmap *bitmap);

/**
 * @brief Create a resident copy of an image cache entry.
 *
 * If there is no RAM for a copy, the entry is pinned and drawn from the PSRAM instead, which is
 * kept retained for as long as the image lives.
 *
 * @param cached Bitmap of an image cache entry, pinned by the caller with a retention hold taken.
 * @return Image, or NULL if out of memory.
 */
ResidentImage *resident_image_create_from_cache(const GBitmap *cached);

/**
 * @brief Create a resident copy of an image cache entry, if there is one.
 *
 * @param client Cache client.
 * @param key Cache key.
 * @param size Size the image must have.
 * @return Image, or NULL on a miss or if out of memory.
 */
ResidentImage *resident_image_load_cached(ImageCacheClient client, const ImageCacheKey *key,
                                          GSize size);

/**
 * @brief Destroy a resident image.
 *
 * @param image Image, or NULL.
 */
void resident_image_destroy(ResidentImage *image);

/**
 * @brief Write pixel data.
 *
 * @param image Image.
 * @param offset Offset in the pixel stream: the rows, each of the format's row size, in order.
 * @param data Data.
 * @param length Length of @p data.
 * @return false if the write goes past the end of the pixels, or the image is a cache entry.
 */
bool resident_image_write(ResidentImage *image, uint32_t offset, const uint8_t *data,
                          size_t length);

/**
 * @brief Get the palette.
 *
 * @param image Image.
 * @return Palette of @ref RESIDENT_IMAGE_MAX_PALETTE entries.
 */
GColor *resident_image_get_palette(ResidentImage *image);

/**
 * @brief Check whether an image is drawn from the image cache, keeping the PSRAM retained.
 *
 * @param image Image.
 * @return true if the image is drawn from the cache, for lack of RAM for a copy.
 */
bool resident_image_is_pinned(const ResidentImage *image);

/**
 * @brief Get the size.
 *
 * @param image Image.
 * @return Size in pixels.
 */
GSize resident_image_get_size(const ResidentImage *image);

/**
 * @brief Get the size of the pixel stream.
 *
 * @param image Image.
 * @return Size in bytes.
 */
uint32_t resident_image_get_data_size(const ResidentImage *image);

/**
 * @brief Draw an image, with the context's compositing mode.
 *
 * @param ctx Graphics context.
 * @param image Image.
 * @param rect Rectangle to draw the image's top left part in, at most the image size.
 */
void resident_image_draw(GContext *ctx, const ResidentImage *image, const GRect *rect);

/** @} */
