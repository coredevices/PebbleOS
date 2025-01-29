/*
   uPNG -- derived from LodePNG version 20100808

   Copyright(c) 2005-2010 Lode Vandevenne
   Copyright(c) 2010 Sean Middleditch
   Copyright(c) 2013-2014 Matthew Hungerford
   Copyright(c) 2015 by Pebble Inc.

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
   */

/* Version history:
   0.0   8 Aug 2010  LodePNG 20100808 release
   1.0  19 Oct 2010  Initial uPNG based on LodePNG 20100808, huffman decoder, SDL/GL viewer
   1.1  11 Dec 2013  Reduced huffman data overhead and moved huffman tables to heap
   1.2  10 Mar 2014  Support non-byte-aligned images (fixes 1,2,4 bit PNG8 support)
   1.3  11 Feb 2015  Add PNG8 alpha_palette support.  Add APNG support (iterative frame decoding)
   1.4  14 Dec 2015  Replace built-in huffman inflate with tinflate (tiny inflate)
 */

#if !defined(UPNG_H)
#define UPNG_H

#include <stdint.h>
#include <stdbool.h>

#include "util/attributes.h"
#include "util/pack.h"

// PNG files start with [137, 'P', 'N', 'G']
#define PNG_SIGNATURE MAKE_WORD('\x89', 'P', 'N', 'G')

#define PNG_HEADER_SIZE 33 // Full header == 8 + 25 (PNG_file_signature + IHDR_CHUNK)
#define CHUNK_META_SIZE 12 // PNG Chunks have 12 bytes of metadata (Length, Type, CRC)
#define FCTL_CHUNK_SIZE (26 + CHUNK_META_SIZE) // FCTL data_size + META_SIZE

#define CHUNK_IHDR MAKE_WORD('I', 'H', 'D', 'R') // Image Header Chunk
#define CHUNK_IDAT MAKE_WORD('I', 'D', 'A', 'T') // Image Data Chunk
#define CHUNK_PLTE MAKE_WORD('P', 'L', 'T', 'E') // Palette Chunk
#define CHUNK_IEND MAKE_WORD('I', 'E', 'N', 'D') // Image End Chunk
#define CHUNK_TRNS MAKE_WORD('t', 'R', 'N', 'S') // Alpha transparency for palettized images
#define CHUNK_ACTL MAKE_WORD('a', 'c', 'T', 'L') // Animation control (APNG)
#define CHUNK_ADTL MAKE_WORD('a', 'd', 'T', 'L') // Animation duration (APNG)
#define CHUNK_FDAT MAKE_WORD('f', 'd', 'A', 'T') // Frame Data (APNG)
#define CHUNK_FCTL MAKE_WORD('f', 'c', 'T', 'L') // Frame control (APNG)

#define APNG_DEFAULT_DELAY_UNITS (100)  // APNG default delay units (ie. 1/100 per frame)

typedef enum upng_error {
  UPNG_EOK   = 0, // success(no error)
  UPNG_ENOMEM   = 1, // memory allocation failed
  UPNG_ENOTFOUND  = 2, // resource not found(file missing)
  UPNG_ENOTPNG  = 3, // image data does not have a PNG header
  UPNG_EMALFORMED  = 4, // image data is not a valid PNG image
  UPNG_EUNSUPPORTED = 5, // critical PNG chunk type is not supported
  UPNG_EUNINTERLACED = 6, // image int32_terlacing is not supported
  UPNG_EUNFORMAT  = 7, // image color format is not supported
  UPNG_EPARAM   = 8, // invalid parameter to method call
  UPNG_EDONE   = 9 // completed decoding all information to end of file (IEND)
} upng_error;

typedef enum upng_format {
  UPNG_BADFORMAT,
  UPNG_INDEXED1,
  UPNG_INDEXED2,
  UPNG_INDEXED4,
  UPNG_INDEXED8,
  UPNG_RGB8,
  UPNG_RGB16,
  UPNG_RGBA8,
  UPNG_RGBA16,
  UPNG_LUMINANCE1,
  UPNG_LUMINANCE2,
  UPNG_LUMINANCE4,
  UPNG_LUMINANCE8,
  UPNG_LUMINANCE_ALPHA1,
  UPNG_LUMINANCE_ALPHA2,
  UPNG_LUMINANCE_ALPHA4,
  UPNG_LUMINANCE_ALPHA8
} upng_format;

typedef struct upng_t upng_t;

typedef struct PACKED rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} rgb;


upng_t*  upng_create(void);
void  upng_destroy(upng_t* upng, bool free_image_buffer);

void  upng_load_bytes(upng_t* upng, const uint8_t* source_buffer, uint32_t source_size);

upng_error upng_decode_metadata(upng_t* upng);
upng_error upng_decode_image(upng_t* upng);

upng_error upng_get_error(const upng_t* upng);
uint32_t upng_get_error_line(const upng_t* upng);

uint32_t upng_get_width(const upng_t* upng);
uint32_t upng_get_height(const upng_t* upng);
uint32_t upng_get_bpp(const upng_t* upng);
uint32_t upng_get_bitdepth(const upng_t* upng);
uint32_t upng_get_components(const upng_t* upng);
uint32_t upng_get_pixelsize(const upng_t* upng);
upng_format upng_get_format(const upng_t* upng);
uint32_t upng_get_size(const upng_t* upng);

// returns palette and count of entries in palette for indexed images
uint16_t upng_get_palette(const upng_t *upng, rgb **palette);

// returns(optional) alpha_palette and count of entries in alpha_palette for indexed images
uint16_t upng_get_alpha_palette(const upng_t *upng, uint8_t **alpha_palette);

const uint8_t* upng_get_buffer(const upng_t* upng);

typedef enum apng_dispose_ops {
  APNG_DISPOSE_OP_NONE = 0,
  APNG_DISPOSE_OP_BACKGROUND,
  APNG_DISPOSE_OP_PREVIOUS
} apng_dispose_ops;

typedef enum apng_blend_ops {
  APNG_BLEND_OP_SOURCE = 0,
  APNG_BLEND_OP_OVER
} apng_blend_ops;

typedef struct PACKED apng_fctl {
  uint32_t sequence_number;
  uint32_t width;
  uint32_t height;
  uint32_t x_offset;
  uint32_t y_offset;
  uint16_t delay_num;
  uint16_t delay_den;
  apng_dispose_ops dispose_op : 8;
  apng_blend_ops blend_op : 8;
} apng_fctl;

// returns if the png is an apng after the upng_load() function
bool upng_is_apng(const upng_t* upng);

// retuns the apng num_frames
uint32_t upng_apng_num_frames(const upng_t* upng);

// retuns the apng num_plays (0 indicates infinite looping)
uint32_t upng_apng_num_plays(const upng_t* upng);

// Pass in a apng_fctl to get the next frames frame control information
bool upng_get_apng_fctl(const upng_t* upng, apng_fctl *apng_frame_control);

#endif /* defined(UPNG_H) */
