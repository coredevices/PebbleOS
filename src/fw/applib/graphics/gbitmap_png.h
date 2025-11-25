/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "gtypes.h"

#include "upng.h"
#include <stdint.h>
#include <stdbool.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup Resources
//!   @{
//!     @addtogroup FileFormats File Formats
//!     @{
//!       @addtogroup PNGFileFormat PNG8 File Format
//!
//! Pebble supports both a PBIs (uncompressed bitmap images) as well as PNG8 images.
//! PNG images are compressed allowing for storage savings up to 90%.
//! PNG8 is a PNG that uses palette-based or grayscale images with 1, 2, 4 or 8 bits per pixel.
//! For palette-based images the pixel data represents the index into the palette, such
//! that each pixel only needs to be large enough to represent the palette size, so
//! \li \c 1-bit supports up to 2 colors,
//! \li \c 2-bit supports up to 4 colors,
//! \li \c 4-bit supports up to 16 colors,
//! \li \c 8-bit supports up to 256 colors.
//!
//! There are 2 parts to the palette: the RGB24 color-mapping palette ("PLTE"), and the optional
//! 8-bit transparency palette ("tRNs").  A pixel's color index maps to both tables, combining to
//! allow the pixel to have both color as well as transparency.
//!
//! For grayscale images, the pixel data represents the luminosity (or shade of gray).
//! \li \c 1-bit supports black and white
//! \li \c 2-bit supports black, dark_gray, light_gray and white
//! \li \c 4-bit supports black, white and 14 shades of gray
//! \li \c 8-bit supports black, white and 254 shades of gray
//!
//! Optionally, grayscale images allow for 1 fully transparent color, which is removed from
//! the fully-opaque colors above (e.g. a 2 bit grayscale image can have black, white, dark_gray
//! and a transparent color).
//!
//! The Basalt Platform provides for 2-bits per color channel, so images are optimized by the
//! SDK tooling when loaded as a resource-type "png" to the Pebble's 64-colors with 4 levels
//! of transparency.  This optimization also handles mapping unsupported colors to the nearest
//! supported color, and reducing the pixel depth to the number of bits required to support
//! the optimized number of colors.  PNG8 images from other sources are supported, with the colors
//! truncated to match supported colors at runtime.
//!
//! @see \ref gbitmap_create_from_png_data
//! @see \ref gbitmap_create_with_resource
//!
//!       @{
//!       @} // end addtogroup png_file_format
//!     @} // end addtogroup FileFormats
//!   @} // end addtogroup Resources
//! @} // end addtogroup Foundation

//! This function scans the data array for the PNG file signature
//! @param data to check for the PNG signature
//! @param data_size size of data array in bytes
//! @return True if the data starts with a PNG file signature
bool gbitmap_png_data_is_png(const uint8_t *data, size_t data_size);

//! @addtogroup Graphics
//! @{
//!   @addtogroup GraphicsTypes Graphics Types
//!   @{

//! Create a \ref GBitmap based on raw PNG data.
//! The resulting \ref GBitmap must be destroyed using \ref gbitmap_destroy().
//! The developer is responsible for freeing png_data following this call.
//! @note PNG decoding currently supports 1,2,4 and 8 bit palettized and grayscale images.
//! @param png_data PNG image data.
//! @param png_data_size PNG image size in bytes.
//! @return A pointer to the \ref GBitmap. `NULL` if the \ref GBitmap could not
//! be created
GBitmap* gbitmap_create_from_png_data(const uint8_t *png_data, size_t png_data_size);

bool gbitmap_init_with_png_data(GBitmap *bitmap, const uint8_t *data, size_t data_size);

//!   @} // end addtogroup GraphicsTypes
//! @} // end addtogroup Graphics

//! This function retrieves a GColor8 color palette from a PNG loaded by uPNG
//! @param upng Pointer to upng containing loaded PNG data
//! @param[out] palette_out Handle to GColor8 palette to allocate and fill with GColor8 palette
//! @return Count of colors in palette, 0 otherwise
uint16_t gbitmap_png_load_palette(upng_t *upng, GColor8 **palette_out);

//! This function retrieves a transparent gray matching value from a PNG loaded by uPNG
//! @param upng Pointer to upng containing loaded PNG data
//! @return Transparent gray value for grayscale PNGs if found, -1 otherwise
int32_t gbitmap_png_get_transparent_gray_value(upng_t *upng);

//! This function checks if the format of the loaded upng header is supported
//! @param upng Pointer to upng containing loaded PNG header
//! @return True if supported, False otherwise
bool gbitmap_png_is_format_supported(upng_t *upng);

//! @internal
int32_t png_seek_chunk_in_resource(uint32_t resource_id, uint32_t offset,
                                   bool seek_framedata, bool *found_actl);

//! @internal
//! This function returns the distance from an offset in a resource, from the specified app number,
//! to next IDAT/fdAT chunk including that chunks data
//! @param app_num the app resource space from which to read the resource
//! @param resource_id Resource to seek for PNG/APNG informational chunks
//! @param offset Position in resource (in bytes) to start seeking from
//! @param seek_framedata Option to seek framedata (FDAT/IDAT)
//! or framedata and frame control (FCTL/IDAT)
//! @param found_actl if not NULL, contains if the actl chunk was encountered during seeking
//! @return If seek_framedata is true, returns offset to FDAT or IDAT chunk
//! including chunk data size, otherwise returns offset to FCTL or IDAT
//! not including those chunks data size
int32_t png_seek_chunk_in_resource_system(ResAppNum app_num, uint32_t resource_id, uint32_t offset,
                                          bool seek_framedata, bool *found_actl);
