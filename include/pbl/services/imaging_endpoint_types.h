/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include "pbl/kernel/compiler.h"

/**
 * @addtogroup services_imaging
 * @{
 */

/** @brief Image-fetch endpoint (0x0035) command identifiers. */
typedef enum {
  /** Image request, watch to phone. */
  ImagingCmdIDRequest = 0x01,
  /** Image response chunk, phone to watch. */
  ImagingCmdIDResponse = 0x02,

  /** Invalid command. */
  ImagingCmdIDInvalid = 0xff,
} ImagingCmdID;

/** @brief What an image is for; determines the type-specific parameters of the request. */
typedef enum {
  /** Album art of a music track. */
  ImagingImageTypeAlbumArt = 0x00,
  /** Image attached to a notification. */
  ImagingImageTypeNotification = 0x01,

  /** Number of image types. */
  ImagingImageTypeCount,
} ImagingImageType;

/** @brief Pixel encoding requested by the watch, and used by the response. */
typedef enum {
  /** 1-bpp black and white. */
  ImagingFormat1Bit = 0x00,
  /** 8-bpp GColor8. */
  ImagingFormat8BitColor = 0x01,
  /** 4-bpp palettized GColor8, up to 16 colors. */
  ImagingFormat4BitPalette = 0x02,
} ImagingFormat;

/**
 * @brief Image request header, watch to phone.
 *
 * Type-specific parameters follow:
 * - For @ref ImagingImageTypeAlbumArt, @c uint8_t title length, title, @c uint8_t artist
 *   length and artist. The phone returns art for that track, so the request cannot race a track
 *   change, or no image.
 * - For @ref ImagingImageTypeNotification, the 16-byte UUID of the timeline item, as the phone
 *   keyed its cache.
 */
typedef struct PBL_PACKED {
  /** @ref ImagingCmdIDRequest. */
  uint8_t cmd;
  /** Opaque value echoed in the response to match it to the request. */
  uint8_t token;
  /** @ref ImagingImageType. */
  uint8_t image_type;
  /** Requested @ref ImagingFormat. */
  uint8_t format;
  /** Desired width in pixels. */
  uint16_t width;
  /** Desired height in pixels. */
  uint16_t height;
  // Type-specific parameters follow. For ImagingImageTypeAlbumArt:
  //   uint8_t title_len;  char title[title_len];
  //   uint8_t artist_len; char artist[artist_len];
  // The phone returns art for the named track (so it can't race a track change), or NO_IMAGE.
  // For ImagingImageTypeNotification:
  //   uint8_t item_id[16];  // the timeline item's UUID, as the phone keyed its cache
} ImagingRequestHeader;

/** @brief Flags of an image response chunk. */
typedef enum {
  /** First chunk; the image header precedes the pixels. */
  ImagingResponseFlagFirst = (1 << 0),
  /** Last chunk of the transfer. */
  ImagingResponseFlagLast = (1 << 1),
  /** The phone has no image; no pixels follow. */
  ImagingResponseFlagNoImage = (1 << 2),
  /**
   * The phone cannot serve this image type; no pixels follow. The watch stops requesting the type
   * for the rest of the connection.
   */
  ImagingResponseFlagUnsupported = (1 << 3),
} ImagingResponseFlags;

/**
 * @brief Mask of the @ref ImagingImageType a response answers, in bits 4-7 of its flags.
 *
 * Several consumers can have a request outstanding at once, and the token alone does not tell
 * which one a response is for.
 */
#define IMAGING_RESPONSE_FLAG_TYPE_MASK (0xf0)
/** @brief Shift of the image type in a response's flags. */
#define IMAGING_RESPONSE_FLAG_TYPE_SHIFT (4)

/**
 * @brief Image response chunk header, phone to watch.
 *
 * @ref chunk_len pixel bytes follow. On the first chunk they are preceded by an image header:
 * @c uint16_t width, @c uint16_t height, @c uint8_t @ref ImagingFormat, @c uint8_t palette count
 * (1-16 for palette formats, else 0) and that many GColor8 palette entries.
 */
typedef struct PBL_PACKED {
  /** @ref ImagingCmdIDResponse. */
  uint8_t cmd;
  /** Token of the request. */
  uint8_t token;
  /** @ref ImagingResponseFlags and the image type. */
  uint8_t flags;
  /** Byte offset of this chunk's pixels in the pixel stream. */
  uint32_t offset;
  /** Number of pixel bytes in this chunk. */
  uint16_t chunk_len;
  // First chunk only, before the pixel data:
  //   uint16_t width;
  //   uint16_t height;
  //   uint8_t  format;          // ImagingFormat
  //   uint8_t  palette_count;   // palette formats only: 1..16 (0 for non-palette)
  //   uint8_t  palette[palette_count];  // GColor8 entries
} ImagingResponseHeader;

/** @} */
