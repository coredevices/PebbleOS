/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/imaging.h"

#include "applib/graphics/gtypes.h"
#include "kernel/kernel_heap.h"
#include "kernel/pbl_malloc.h"
#include "pbl/kernel/mutex.h"
#include "pbl/logging/logging.h"
#include "pbl/services/comm_session/session.h"
#include "pbl/util/heap.h"
#include "pbl/util/math.h"
#include "pbl/util/size.h"

#include <inttypes.h>
#include <string.h>

PBL_LOG_MODULE_DEFINE(service_imaging, DEFAULT_LOG_LEVEL);

static const uint16_t IMAGING_ENDPOINT = 0x35;

// A full-screen 260x260 cover is 33,800 packed bytes. Segment album art so no allocation grows
// with that total, and cap the encoded stream against malformed or hostile responses.
#define IMAGING_MAX_BYTES         (40 * 1024)
#define IMAGING_MAX_DIM           (300)
#define IMAGING_PALETTE_ENTRIES   (16)
#define IMAGING_ART_SEGMENT_BYTES (512)
#define IMAGING_ART_SEGMENTS \
  ((IMAGING_MAX_BYTES + IMAGING_ART_SEGMENT_BYTES - 1) / IMAGING_ART_SEGMENT_BYTES)
#define IMAGING_ART_TILE_ROWS (10)
#define IMAGING_ART_MAX_TILES \
  ((IMAGING_MAX_DIM + IMAGING_ART_TILE_ROWS - 1) / IMAGING_ART_TILE_ROWS)

struct ImagingAlbumArt {
  uint8_t *segments[IMAGING_ART_SEGMENTS];
  uint32_t tile_offsets[IMAGING_ART_MAX_TILES];
  uint8_t palette[IMAGING_PALETTE_ENTRIES];
  uint32_t length;
  uint16_t width;
  uint16_t height;
  uint16_t row_size;
  uint8_t palette_count;
  bool compressed;
};

static ImagingAlbumArtHandler s_album_art_handler;

void imaging_register_album_art_handler(ImagingAlbumArtHandler handler) {
  s_album_art_handler = handler;
}

void imaging_album_art_free(ImagingAlbumArt *art) {
  if (!art) {
    return;
  }
  for (unsigned int i = 0; i < ARRAY_LENGTH(art->segments); ++i) {
    kernel_free(art->segments[i]);
  }
  kernel_free(art);
}

uint16_t imaging_album_art_width(const ImagingAlbumArt *art) {
  return art->width;
}
uint16_t imaging_album_art_height(const ImagingAlbumArt *art) {
  return art->height;
}
uint16_t imaging_album_art_row_size(const ImagingAlbumArt *art) {
  return art->row_size;
}
const uint8_t *imaging_album_art_palette(const ImagingAlbumArt *art) {
  return art->palette;
}

static bool prv_art_read(const ImagingAlbumArt *art, uint32_t offset, uint8_t *out) {
  if (offset >= art->length) {
    return false;
  }
  *out = art->segments[offset / IMAGING_ART_SEGMENT_BYTES][offset % IMAGING_ART_SEGMENT_BYTES];
  return true;
}

static bool prv_art_copy(const ImagingAlbumArt *art, uint32_t offset, uint8_t *out, size_t length) {
  if (offset > art->length || length > art->length - offset) {
    return false;
  }
  while (length) {
    const size_t segment_offset = offset % IMAGING_ART_SEGMENT_BYTES;
    const size_t n = MIN(length, IMAGING_ART_SEGMENT_BYTES - segment_offset);
    memcpy(out, art->segments[offset / IMAGING_ART_SEGMENT_BYTES] + segment_offset, n);
    out += n;
    offset += n;
    length -= n;
  }
  return true;
}

//! Bounded LZ4 block decoder; source remains in segmented storage.
static bool prv_art_lz4(const ImagingAlbumArt *art, uint32_t start, uint32_t length,
                        uint8_t *output, size_t output_size) {
  uint32_t src = start;
  const uint32_t end = start + length;
  size_t dst = 0;
  while (src < end) {
    uint8_t token;
    if (!prv_art_read(art, src++, &token)) {
      return false;
    }
    size_t literals = token >> 4;
    if (literals == 15) {
      uint8_t extra;
      do {
        if (src >= end || !prv_art_read(art, src++, &extra))
          return false;
        literals += extra;
        if (literals > output_size)
          return false;
      } while (extra == 255);
    }
    if (literals > output_size - dst || literals > end - src ||
        !prv_art_copy(art, src, output + dst, literals))
      return false;
    src += literals;
    dst += literals;
    if (src == end)
      return dst == output_size;
    uint8_t lo, hi;
    if (end - src < 2 || !prv_art_read(art, src++, &lo) || !prv_art_read(art, src++, &hi))
      return false;
    const size_t back = lo | (hi << 8);
    if (!back || back > dst)
      return false;
    size_t match = (token & 15) + 4;
    if ((token & 15) == 15) {
      uint8_t extra;
      do {
        if (src >= end || !prv_art_read(art, src++, &extra))
          return false;
        match += extra;
        if (match > output_size)
          return false;
      } while (extra == 255);
    }
    if (match > output_size - dst)
      return false;
    for (size_t i = 0; i < match; ++i)
      output[dst + i] = output[dst + i - back];
    dst += match;
  }
  return false;
}

bool imaging_album_art_decode_tile(const ImagingAlbumArt *art, uint16_t tile, uint8_t *output,
                                   size_t output_size) {
  if (!art || !output ||
      tile >= (art->height + IMAGING_ART_TILE_ROWS - 1) / IMAGING_ART_TILE_ROWS) {
    return false;
  }
  const uint16_t rows = MIN(IMAGING_ART_TILE_ROWS, art->height - tile * IMAGING_ART_TILE_ROWS);
  const size_t raw_size = (size_t)art->row_size * rows;
  if (output_size < raw_size)
    return false;
  if (!art->compressed) {
    return prv_art_copy(art, (uint32_t)tile * IMAGING_ART_TILE_ROWS * art->row_size, output,
                        raw_size);
  }
  const uint32_t offset = art->tile_offsets[tile];
  uint8_t lo, hi;
  if (!prv_art_read(art, offset, &lo) || !prv_art_read(art, offset + 1, &hi))
    return false;
  const uint16_t header = lo | (hi << 8);
  const size_t length = header & 0x7fff;
  const uint32_t start = offset + 2;
  if (!length || start > art->length || length > art->length - start)
    return false;
  if (header & 0x8000) {
    return length == raw_size && prv_art_copy(art, start, output, raw_size);
  }
  return prv_art_lz4(art, start, length, output, raw_size);
}

static bool prv_art_validate(ImagingAlbumArt *art) {
  if (!art->compressed)
    return true;
  uint8_t *scratch = kernel_malloc(1500);
  if (!scratch)
    return false;
  uint32_t offset = 0;
  bool valid = true;
  const uint16_t tiles = (art->height + IMAGING_ART_TILE_ROWS - 1) / IMAGING_ART_TILE_ROWS;
  for (uint16_t tile = 0; tile < tiles; ++tile) {
    uint8_t lo, hi;
    if (offset + 2 > art->length || !prv_art_read(art, offset, &lo) ||
        !prv_art_read(art, offset + 1, &hi)) {
      valid = false;
      break;
    }
    const uint32_t length = (lo | (hi << 8)) & 0x7fff;
    if (!length || offset + 2 + length > art->length) {
      valid = false;
      break;
    }
    art->tile_offsets[tile] = offset;
    if (!imaging_album_art_decode_tile(art, tile, scratch, 1500)) {
      valid = false;
      break;
    }
    offset += 2 + length;
  }
  kernel_free(scratch);
  return valid && offset == art->length;
}

static bool prv_art_append(ImagingAlbumArt *art, uint32_t offset, const uint8_t *data,
                           size_t length) {
  while (length) {
    const size_t index = offset / IMAGING_ART_SEGMENT_BYTES;
    const size_t part_offset = offset % IMAGING_ART_SEGMENT_BYTES;
    const size_t n = MIN(length, IMAGING_ART_SEGMENT_BYTES - part_offset);
    if (!art->segments[index]) {
      art->segments[index] = kernel_malloc(IMAGING_ART_SEGMENT_BYTES);
      if (!art->segments[index])
        return false;
    }
    memcpy(art->segments[index] + part_offset, data, n);
    data += n;
    offset += n;
    length -= n;
  }
  return true;
}

static ImagingReceivedHandler s_handlers[ImagingImageTypeCount];
static ImagingWillReceiveHandler s_will_receive_handlers[ImagingImageTypeCount];
static ImagingTransferFailedHandler s_transfer_failed_handlers[ImagingImageTypeCount];

//! Guards the latch state below: requests come in on the requesting task (e.g. the Music app)
//! while responses are handled on KernelMain. The reassembly state (s_rx) is deliberately not
//! covered — it is only ever touched on KernelMain (endpoint receiver and comm-session events).
static PBL_MUTEX_DEFINE(s_lock);

// Image types the phone told us it can't serve (ImagingResponseFlagUnsupported), so we stop asking.
// Latched per session: the connected phone doesn't change what it supports mid-connection, and a
// reconnect (possibly to a different phone) clears it via prv_session_types.
static uint32_t s_unsupported_types;
static CommSession *s_latched_session;

static struct {
  bool active;
  uint8_t token;
  uint8_t type;
  GBitmapFormat format;
  uint16_t width;
  uint16_t height;
  uint16_t row_size_bytes;
  uint32_t total_bytes;
  uint32_t received_bytes;
  uint8_t *pixels;
  GColor *palette; // NULL for non-palette formats
  ImagingAlbumArt *art;
} s_rx;

static void prv_rx_reset(void) {
  kernel_free(s_rx.pixels);
  kernel_free(s_rx.palette);
  imaging_album_art_free(s_rx.art);
  s_rx = (__typeof__(s_rx)){0};
}

void imaging_register_handler(ImagingImageType image_type, ImagingReceivedHandler handler) {
  if (image_type < ARRAY_LENGTH(s_handlers)) {
    s_handlers[image_type] = handler;
  }
}

void imaging_register_transfer_handlers(ImagingImageType image_type,
                                        ImagingWillReceiveHandler will_receive,
                                        ImagingTransferFailedHandler transfer_failed) {
  if (image_type < ImagingImageTypeCount) {
    s_will_receive_handlers[image_type] = will_receive;
    s_transfer_failed_handlers[image_type] = transfer_failed;
  }
}

static void prv_drop(uint8_t type, uint8_t token, uint32_t size, const char *reason) {
  unsigned int used;
  unsigned int free_bytes;
  unsigned int largest_free;
  heap_calc_totals(kernel_heap_get(), &used, &free_bytes, &largest_free);
  PBL_LOG_WRN("Drop %s token=%u size=%" PRIu32 " largest=%u", reason, token, size, largest_free);

  prv_rx_reset();
  ImagingTransferFailedHandler handler =
      (type < ARRAY_LENGTH(s_transfer_failed_handlers)) ? s_transfer_failed_handlers[type] : NULL;
  if (handler) {
    handler(token);
  }
}

//! The type a response answers, from the top nibble of its flags byte. Zero — which is what a phone
//! that doesn't set those bits sends — is album art. A value we don't know is left out of range so
//! it routes nowhere rather than to the wrong consumer.
static uint8_t prv_response_type(const ImagingResponseHeader *hdr) {
  return (hdr->flags & IMAGING_RESPONSE_FLAG_TYPE_MASK) >> IMAGING_RESPONSE_FLAG_TYPE_SHIFT;
}

static void prv_deliver(uint8_t token, uint8_t type, GBitmap *bitmap, ImagingAlbumArt *art) {
  if (type == ImagingImageTypeAlbumArt && s_album_art_handler && !bitmap) {
    s_album_art_handler(token, art);
    return;
  }
  imaging_album_art_free(art);
  ImagingReceivedHandler handler = (type < ARRAY_LENGTH(s_handlers)) ? s_handlers[type] : NULL;
  if (handler) {
    handler(token, bitmap);
  } else if (bitmap) {
    kernel_free(bitmap->addr);
    kernel_free(bitmap->palette);
    kernel_free(bitmap);
  }
}

// Clear the unsupported-type latch when the system session changes (reconnect / different phone).
// The latch is also cleared explicitly when the session closes (imaging_handle_comm_session_event)
// so a recycled session pointer can't be mistaken for the old one. s_lock held by the caller.
static bool prv_type_latched_unsupported(CommSession *session, ImagingImageType image_type) {
  if (session != s_latched_session) {
    s_latched_session = session;
    s_unsupported_types = 0;
  }
  return (s_unsupported_types & (1u << image_type)) != 0;
}

bool imaging_is_type_supported(ImagingImageType image_type) {
  CommSession *session = comm_session_get_system_session();
  if (!session || !comm_session_has_capability(session, CommSessionImagingSupport)) {
    return false;
  }
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool latched = prv_type_latched_unsupported(session, image_type);
  pbl_mutex_unlock(&s_lock);
  return !latched;
}

bool imaging_request_album_art(uint8_t token, ImagingFormat format, uint16_t width, uint16_t height,
                               const char *title, const char *artist) {
  if (!imaging_is_type_supported(ImagingImageTypeAlbumArt)) {
    return false;
  }
  CommSession *session = comm_session_get_system_session();
  const size_t title_len = title ? MIN(strlen(title), 255) : 0;
  const size_t artist_len = artist ? MIN(strlen(artist), 255) : 0;
  uint8_t payload[sizeof(ImagingRequestHeader) + 2 + 255 + 255];
  ImagingRequestHeader *hdr = (ImagingRequestHeader *)payload;
  hdr->cmd = ImagingCmdIDRequest;
  hdr->token = token;
  hdr->image_type = ImagingImageTypeAlbumArt;
  hdr->format = format;
  hdr->width = width;
  hdr->height = height;
  uint8_t *cursor = payload + sizeof(*hdr);
  *cursor++ = (uint8_t)title_len;
  memcpy(cursor, title, title_len);
  cursor += title_len;
  *cursor++ = (uint8_t)artist_len;
  memcpy(cursor, artist, artist_len);
  cursor += artist_len;

  // Temporary INFO diagnostics for on-watch album-art testing.
  PBL_LOG_INFO("Art request token=%u fmt=%u %ux%u", token, (unsigned int)format, width, height);
  comm_session_send_data(session, IMAGING_ENDPOINT, payload, cursor - payload,
                         COMM_SESSION_DEFAULT_TIMEOUT);
  return true;
}

bool imaging_request_notification_image(uint8_t token, ImagingFormat format, uint16_t width,
                                        uint16_t height, const Uuid *item_id) {
  if (!item_id || !imaging_is_type_supported(ImagingImageTypeNotification)) {
    return false;
  }
  CommSession *session = comm_session_get_system_session();
  uint8_t payload[sizeof(ImagingRequestHeader) + UUID_SIZE];
  ImagingRequestHeader *hdr = (ImagingRequestHeader *)payload;
  hdr->cmd = ImagingCmdIDRequest;
  hdr->token = token;
  hdr->image_type = ImagingImageTypeNotification;
  hdr->format = format;
  hdr->width = width;
  hdr->height = height;
  memcpy(payload + sizeof(*hdr), item_id, UUID_SIZE);

  comm_session_send_data(session, IMAGING_ENDPOINT, payload, sizeof(payload),
                         COMM_SESSION_DEFAULT_TIMEOUT);
  return true;
}

static uint16_t prv_gbitmap_format_for(ImagingFormat format, GBitmapFormat *out) {
  switch (format) {
    case ImagingFormat8BitColor:
      *out = GBitmapFormat8Bit;
      return 0; // no palette
    case ImagingFormat4BitPalette:
      *out = GBitmapFormat4BitPalette;
      return IMAGING_PALETTE_ENTRIES;
    case ImagingFormat1Bit:
    default:
      *out = GBitmapFormat1Bit;
      return 0;
  }
}

void imaging_protocol_msg_callback(CommSession *session, const uint8_t *msg, size_t length) {
  if (length < sizeof(ImagingResponseHeader)) {
    return;
  }
  const ImagingResponseHeader *hdr = (const ImagingResponseHeader *)msg;
  if (hdr->cmd != ImagingCmdIDResponse) {
    return;
  }
  const uint8_t *cursor = msg + sizeof(*hdr);
  const uint8_t *msg_end = msg + length;

  const uint8_t type = prv_response_type(hdr);

  if (hdr->flags & ImagingResponseFlagUnsupported) {
    // Phone can't serve this type: latch it off so we don't ask again this connection, and deliver
    // NULL so the current request resolves (the app treats it like "no image").
    if (type < ImagingImageTypeCount) {
      pbl_mutex_lock(&s_lock, PBL_FOREVER);
      s_unsupported_types |= (1u << type);
      pbl_mutex_unlock(&s_lock);
    }
    prv_rx_reset();
    prv_deliver(hdr->token, type, NULL, NULL);
    return;
  }

  if (hdr->flags & ImagingResponseFlagNoImage) {
    prv_rx_reset();
    prv_deliver(hdr->token, type, NULL, NULL);
    return;
  }

  if (hdr->flags & ImagingResponseFlagFirst) {
    if (s_rx.active) {
      prv_drop(s_rx.type, s_rx.token, s_rx.total_bytes, "superseded");
    }
    if ((size_t)(msg_end - cursor) < 6) {
      prv_drop(type, hdr->token, length, "short image header");
      return;
    }
    const uint16_t width = cursor[0] | (cursor[1] << 8);
    const uint16_t height = cursor[2] | (cursor[3] << 8);
    const uint8_t format = cursor[4];
    const uint8_t palette_count = cursor[5];
    cursor += 6;
    GBitmapFormat gformat;
    const bool compressed = (format == ImagingFormat4BitPaletteLz4);
    if (compressed && type != ImagingImageTypeAlbumArt) {
      prv_drop(type, hdr->token, length, "compressed non-album image");
      return;
    }
    const uint16_t max_palette =
        compressed ? IMAGING_PALETTE_ENTRIES : prv_gbitmap_format_for(format, &gformat);
    if (compressed)
      gformat = GBitmapFormat4BitPalette;
    if (width == 0 || height == 0 || width > IMAGING_MAX_DIM || height > IMAGING_MAX_DIM ||
        palette_count > max_palette || (max_palette > 0 && palette_count == 0)) {
      prv_drop(type, hdr->token, length, "invalid image metadata");
      return;
    }
    if ((size_t)(msg_end - cursor) < palette_count) {
      prv_drop(type, hdr->token, length, "short palette");
      return;
    }
    const uint16_t row_size = gbitmap_format_get_row_size_bytes(width, gformat);
    const uint32_t raw_total = (uint32_t)row_size * height;
    uint32_t total = raw_total;
    if (compressed) {
      if ((size_t)(msg_end - cursor) < (size_t)palette_count + 4) {
        prv_drop(type, hdr->token, length, "short compressed length");
        return;
      }
      const uint8_t *encoded_length = cursor + palette_count;
      total = (uint32_t)encoded_length[0] | ((uint32_t)encoded_length[1] << 8) |
              ((uint32_t)encoded_length[2] << 16) | ((uint32_t)encoded_length[3] << 24);
    }
    if (total == 0 || total > IMAGING_MAX_BYTES) {
      prv_drop(type, hdr->token, total, "invalid image size");
      return;
    }

    ImagingWillReceiveHandler will_receive =
        (type < ARRAY_LENGTH(s_will_receive_handlers)) ? s_will_receive_handlers[type] : NULL;
    if (will_receive) {
      will_receive(hdr->token);
    }

    const bool segmented_art = type == ImagingImageTypeAlbumArt && s_album_art_handler &&
                               gformat == GBitmapFormat4BitPalette;
    if (compressed && !segmented_art) {
      prv_drop(type, hdr->token, total, "compressed art without handler");
      return;
    }
    if (segmented_art) {
      s_rx.art = kernel_zalloc(sizeof(*s_rx.art));
      if (!s_rx.art) {
        prv_drop(type, hdr->token, total, "art allocation failed");
        return;
      }
      s_rx.art->length = total;
      s_rx.art->width = width;
      s_rx.art->height = height;
      s_rx.art->row_size = row_size;
      s_rx.art->palette_count = palette_count;
      s_rx.art->compressed = compressed;
      memcpy(s_rx.art->palette, cursor, palette_count);
      cursor += palette_count + (compressed ? 4 : 0);
    } else {
      s_rx.pixels = kernel_zalloc(total);
      if (!s_rx.pixels) {
        prv_drop(type, hdr->token, total, "pixel allocation failed");
        return;
      }
    }
    if (max_palette > 0 && !segmented_art) {
      s_rx.palette = kernel_zalloc(IMAGING_PALETTE_ENTRIES * sizeof(GColor));
      if (!s_rx.palette) {
        prv_drop(type, hdr->token, total, "palette allocation failed");
        return;
      }
      for (uint8_t i = 0; i < palette_count; ++i) {
        s_rx.palette[i] = (GColor){.argb = cursor[i]};
      }
      cursor += palette_count;
    }
    s_rx.active = true;
    s_rx.token = hdr->token;
    s_rx.type = type;
    s_rx.format = gformat;
    s_rx.width = width;
    s_rx.height = height;
    s_rx.row_size_bytes = row_size;
    s_rx.total_bytes = total;
    s_rx.received_bytes = 0;
  }

  if (!s_rx.active || s_rx.token != hdr->token || s_rx.type != type) {
    // ponytail: one reassembly slot, so two consumers fetching at once costs one of them a retry.
    // Add a per-token slot array if that ever matters.
    if (s_rx.active) {
      prv_drop(s_rx.type, s_rx.token, s_rx.total_bytes, "transfer mismatch");
    } else {
      prv_drop(type, hdr->token, length, "no matching transfer");
    }
    return;
  }

  // Reliable, ordered transport: require contiguous in-order chunks with an exact declared length.
  const size_t avail = (cursor <= msg_end) ? (size_t)(msg_end - cursor) : 0;
  if (hdr->offset != s_rx.received_bytes || hdr->chunk_len != avail ||
      (uint32_t)hdr->offset + hdr->chunk_len > s_rx.total_bytes) {
    prv_drop(s_rx.type, s_rx.token, s_rx.total_bytes, "invalid chunk");
    return;
  }
  if (s_rx.art) {
    if (!prv_art_append(s_rx.art, hdr->offset, cursor, hdr->chunk_len)) {
      prv_drop(s_rx.type, s_rx.token, s_rx.total_bytes, "art segment allocation failed");
      return;
    }
  } else {
    memcpy(s_rx.pixels + hdr->offset, cursor, hdr->chunk_len);
  }
  s_rx.received_bytes += hdr->chunk_len;

  if (hdr->flags & ImagingResponseFlagLast) {
    if (s_rx.received_bytes != s_rx.total_bytes) {
      prv_drop(s_rx.type, s_rx.token, s_rx.total_bytes, "incomplete image");
      return;
    }
    if (s_rx.art) {
      if (!prv_art_validate(s_rx.art)) {
        prv_drop(s_rx.type, s_rx.token, s_rx.total_bytes, "invalid art encoding");
        return;
      }
      const uint8_t token = s_rx.token;
      ImagingAlbumArt *art = s_rx.art;
      // Temporary INFO diagnostics; tile headers have already passed validation.
      PBL_LOG_INFO("Art received token=%u fmt=%u %ux%u encoded=%" PRIu32 " raw=%" PRIu32, token,
                   art->compressed ? 3u : 2u, art->width, art->height, art->length,
                   (uint32_t)art->row_size * art->height);
      if (art->compressed) {
        const unsigned int tiles =
            (art->height + IMAGING_ART_TILE_ROWS - 1) / IMAGING_ART_TILE_ROWS;
        unsigned int raw_tiles = 0;
        for (unsigned int tile = 0; tile < tiles; ++tile) {
          uint8_t hi = 0;
          prv_art_read(art, art->tile_offsets[tile] + 1, &hi);
          raw_tiles += (hi & 0x80) != 0;
        }
        PBL_LOG_INFO("Art tiles token=%u lz4=%u raw=%u", token, tiles - raw_tiles, raw_tiles);
      }
      s_rx.art = NULL;
      prv_rx_reset();
      prv_deliver(token, type, NULL, art);
      return;
    }
    GBitmap *bmp = kernel_zalloc(sizeof(GBitmap));
    if (!bmp) {
      prv_drop(s_rx.type, s_rx.token, s_rx.total_bytes, "bitmap allocation failed");
      return;
    }
    bmp->addr = s_rx.pixels;
    bmp->row_size_bytes = s_rx.row_size_bytes;
    bmp->info.format = s_rx.format;
    bmp->info.version = GBITMAP_VERSION_CURRENT;
    bmp->bounds = (GRect){{0, 0}, {s_rx.width, s_rx.height}};
    bmp->palette = s_rx.palette;
    // Ownership of the pixel and palette buffers moves into the bitmap.
    const uint8_t token = s_rx.token;
    s_rx.pixels = NULL;
    s_rx.palette = NULL;
    prv_rx_reset();
    prv_deliver(token, type, bmp, NULL);
  }
}

void imaging_handle_comm_session_event(const PebbleCommSessionEvent *event) {
  if (!event->is_system || event->is_open) {
    return;
  }
  // The system session closed: free any partially received image so an aborted transfer doesn't
  // hold its pixel buffer until the next one starts. This runs on KernelMain, the same task as
  // the endpoint receiver, so touching s_rx is safe. Also clear the unsupported-type latch here
  // rather than relying solely on the pointer comparison in prv_type_latched_unsupported: a
  // future session could be allocated at the address of the freed one.
  if (s_rx.active) {
    prv_drop(s_rx.type, s_rx.token, s_rx.total_bytes, "session closed");
  } else {
    prv_rx_reset();
  }
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_latched_session = NULL;
  s_unsupported_types = 0;
  pbl_mutex_unlock(&s_lock);
}
