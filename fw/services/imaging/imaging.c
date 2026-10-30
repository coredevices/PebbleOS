/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <string.h>

#include <pbl/drivers/rtc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/psram.h>
#include <pbl/logging/logging.h>
#include <pbl/services/comm_session/session.h>
#include <pbl/services/image_cache.h>
#include <pbl/services/imaging.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/services/resident_image.h>
#include <pbl/util/heap.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#include <applib/graphics/gtypes.h>
#include <kernel/event_loop.h>
#include <kernel/kernel_heap.h>

PBL_LOG_MODULE_DEFINE(service_imaging, DEFAULT_LOG_LEVEL);

static const uint16_t IMAGING_ENDPOINT = 0x35;

// Full-screen 4-bpp on the largest supported display (260x260) is ~34 KB. Cap generously and reject
// anything larger to bound memory use against a malformed or hostile phone.
#define IMAGING_MAX_BYTES       (40 * 1024)
#define IMAGING_MAX_DIM         (300)
#define IMAGING_PALETTE_ENTRIES (16)

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

typedef struct {
  bool active;
  uint8_t token;
  uint32_t total_bytes;
  uint32_t received_bytes;
  RtcTicks last_chunk_ticks;
  //! Where the image is received: a cache entry, with a PSRAM hold taken, or a resident image
  ImageCacheEntry *entry;
  ResidentImage *image;
} Reassembly;

//! Cache key of the last request of each type, guarded by s_lock
static struct {
  bool valid;
  uint8_t token;
  ImageCacheKey key;
} s_req[ImagingImageTypeCount];

#ifdef CONFIG_IMAGE_CACHE
#define IS_IMAGE_CACHE_ENABLED true
#else
#define IS_IMAGE_CACHE_ENABLED false
#endif

static const ImageCacheClient CACHE_CLIENTS[ImagingImageTypeCount] = {
  [ImagingImageTypeAlbumArt] = ImageCacheClientAlbumArt,
  [ImagingImageTypeNotification] = ImageCacheClientNotification,
};

// One per type, so that consumers can fetch at the same time
static Reassembly s_rx[ImagingImageTypeCount];

// A transfer the phone stops sending is dropped after this long, not to hold its memory, or keep
// the PSRAM retained, until the next one
#define IMAGING_STALL_TIMEOUT_MS (5000)
static TimerID s_stall_timer = TIMER_INVALID_ID;

static void prv_rx_reset(uint8_t type) {
  if (type < ARRAY_LENGTH(s_rx)) {
    resident_image_destroy(s_rx[type].image);
    if (s_rx[type].entry) {
      image_cache_abort(s_rx[type].entry);
      psram_retain_release();
    }
    s_rx[type] = (Reassembly){};
  }
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

static void prv_drop(uint8_t type, uint8_t token, uint32_t size, const char *reason);
static void prv_arm_stall_timer(void);

static void prv_check_stalled(void *data) {
  const RtcTicks now = rtc_get_ticks();
  bool active = false;

  for (uint8_t type = 0; type < ARRAY_LENGTH(s_rx); type++) {
    Reassembly *rx = &s_rx[type];
    if (!rx->active) {
      continue;
    }
    if ((now - rx->last_chunk_ticks) * 1000 >= (RtcTicks)IMAGING_STALL_TIMEOUT_MS * RTC_TICKS_HZ) {
      prv_drop(type, rx->token, rx->total_bytes, "stalled");
    } else {
      active = true;
    }
  }

  if (active) {
    prv_arm_stall_timer();
  }
}

static void prv_stall_timer_expired(void *data) {
  // The reassembly state is only touched on KernelMain
  launcher_task_add_callback(prv_check_stalled, nullptr);
}

static void prv_arm_stall_timer(void) {
  if (s_stall_timer == TIMER_INVALID_ID) {
    s_stall_timer = new_timer_create();
  }
  new_timer_start(s_stall_timer, IMAGING_STALL_TIMEOUT_MS, prv_stall_timer_expired, nullptr, 0);
}

static void prv_drop(uint8_t type, uint8_t token, uint32_t size, const char *reason) {
  unsigned int used;
  unsigned int free_bytes;
  unsigned int largest_free;
  heap_calc_totals(kernel_heap_get(), &used, &free_bytes, &largest_free);
  PBL_LOG_WRN("Drop %s type=%u token=%u size=%" PRIu32 " largest=%u", reason, type, token, size,
              largest_free);

  prv_rx_reset(type);
  ImagingTransferFailedHandler handler = (type < ARRAY_LENGTH(s_transfer_failed_handlers))
                                             ? s_transfer_failed_handlers[type]
                                             : nullptr;
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

static void prv_deliver(uint8_t token, uint8_t type, ResidentImage *image) {
  ImagingReceivedHandler handler = (type < ARRAY_LENGTH(s_handlers)) ? s_handlers[type] : nullptr;
  if (handler) {
    handler(token, image);
  } else {
    resident_image_destroy(image);
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

static void prv_set_request_key(ImagingImageType type, uint8_t token, const ImageCacheKey *key) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_req[type].valid = (key != nullptr);
  s_req[type].token = token;
  if (key) {
    s_req[type].key = *key;
  }
  pbl_mutex_unlock(&s_lock);
}

static bool prv_get_request_key(uint8_t type, uint8_t token, ImageCacheKey *key) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool valid = s_req[type].valid && (s_req[type].token == token);
  if (valid) {
    *key = s_req[type].key;
  }
  pbl_mutex_unlock(&s_lock);
  return valid;
}

bool imaging_request_album_art(uint8_t token, ImagingFormat format, uint16_t width, uint16_t height,
                               const char *title, const char *artist, const ImageCacheKey *key) {
  if (!imaging_is_type_supported(ImagingImageTypeAlbumArt)) {
    return false;
  }
  prv_set_request_key(ImagingImageTypeAlbumArt, token, key);
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

  comm_session_send_data(session, IMAGING_ENDPOINT, payload, cursor - payload,
                         COMM_SESSION_DEFAULT_TIMEOUT);
  return true;
}

bool imaging_request_notification_image(uint8_t token, ImagingFormat format, uint16_t width,
                                        uint16_t height, const Uuid *item_id) {
  if (!item_id || !imaging_is_type_supported(ImagingImageTypeNotification)) {
    return false;
  }
  ImageCacheKey key;
  image_cache_key_from_uuid(&key, item_id);
  prv_set_request_key(ImagingImageTypeNotification, token, &key);
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
    prv_rx_reset(type);
    prv_deliver(hdr->token, type, nullptr);
    return;
  }

  if (hdr->flags & ImagingResponseFlagNoImage) {
    prv_rx_reset(type);
    prv_deliver(hdr->token, type, nullptr);
    return;
  }

  if (type >= ARRAY_LENGTH(s_rx)) {
    PBL_LOG_WRN("Drop chunk of unknown type=%u", type);
    return;
  }
  Reassembly *rx = &s_rx[type];

  if (hdr->flags & ImagingResponseFlagFirst) {
    if (rx->active) {
      prv_drop(type, rx->token, rx->total_bytes, "superseded");
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
    const uint16_t max_palette = prv_gbitmap_format_for(format, &gformat);
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
    const uint32_t total = (uint32_t)row_size * height;
    if (total == 0 || total > IMAGING_MAX_BYTES) {
      prv_drop(type, hdr->token, total, "invalid image size");
      return;
    }

    ImagingWillReceiveHandler will_receive =
        (type < ARRAY_LENGTH(s_will_receive_handlers)) ? s_will_receive_handlers[type] : nullptr;
    if (will_receive) {
      will_receive(hdr->token);
    }

    GColor palette[IMAGING_PALETTE_ENTRIES] = {};
    for (uint8_t i = 0; i < palette_count; ++i) {
      palette[i] = (GColor){.argb = cursor[i]};
    }
    cursor += palette_count;

    // Receive into the PSRAM cache if possible, else straight into internal RAM
    ImageCacheKey key;
    if (IS_IMAGE_CACHE_ENABLED && prv_get_request_key(type, hdr->token, &key)) {
      psram_retain_acquire();
      rx->entry =
          image_cache_begin(CACHE_CLIENTS[type], &key, GSize(width, height), gformat, max_palette);
      if (rx->entry) {
        memcpy(image_cache_palette(rx->entry), palette, max_palette * sizeof(GColor));
      } else {
        psram_retain_release();
      }
    }
    if (!rx->entry) {
      rx->image = resident_image_create(GSize(width, height), gformat, palette, max_palette);
      if (!rx->image) {
        prv_drop(type, hdr->token, total, "image allocation failed");
        return;
      }
    }
    rx->active = true;
    rx->token = hdr->token;
    rx->total_bytes = total;
    rx->received_bytes = 0;
  }

  if (!rx->active || rx->token != hdr->token) {
    if (rx->active) {
      prv_drop(type, rx->token, rx->total_bytes, "transfer mismatch");
    } else {
      prv_drop(type, hdr->token, length, "no matching transfer");
    }
    return;
  }

  // Reliable, ordered transport: require contiguous in-order chunks with an exact declared length.
  const size_t avail = (cursor <= msg_end) ? (size_t)(msg_end - cursor) : 0;
  if (hdr->offset != rx->received_bytes || hdr->chunk_len != avail ||
      (uint32_t)hdr->offset + hdr->chunk_len > rx->total_bytes) {
    prv_drop(type, rx->token, rx->total_bytes, "invalid chunk");
    return;
  }
  if (rx->entry) {
    memcpy(image_cache_pixels(rx->entry) + hdr->offset, cursor, hdr->chunk_len);
  } else {
    resident_image_write(rx->image, hdr->offset, cursor, hdr->chunk_len);
  }
  rx->received_bytes += hdr->chunk_len;
  rx->last_chunk_ticks = rtc_get_ticks();
  if (!(hdr->flags & ImagingResponseFlagLast)) {
    prv_arm_stall_timer();
  }

  if (hdr->flags & ImagingResponseFlagLast) {
    if (rx->received_bytes != rx->total_bytes) {
      prv_drop(type, rx->token, rx->total_bytes, "incomplete image");
      return;
    }
    ResidentImage *image = rx->image;
    const uint8_t token = rx->token;
    rx->image = nullptr;
    if (rx->entry) {
      // Hand over a copy in internal RAM, so that the PSRAM need not be retained while it's shown
      const GBitmap *cached = image_cache_commit(rx->entry);
      rx->entry = nullptr;
      image = resident_image_create_from_cache(cached);
      image_cache_release(cached);
      psram_retain_release();
      if (!image) {
        prv_drop(type, token, rx->total_bytes, "image copy failed");
        return;
      }
    }
    // Ownership of the image moves to the handler
    prv_rx_reset(type);
    prv_deliver(token, type, image);
  }
}

void imaging_handle_comm_session_event(const PebbleCommSessionEvent *event) {
  if (!event->is_system || event->is_open) {
    return;
  }
  // The system session closed: free any partially received image so an aborted transfer doesn't
  // hold its pixels until the next one starts. This runs on KernelMain, the same task as
  // the endpoint receiver, so touching s_rx is safe. Also clear the unsupported-type latch here
  // rather than relying solely on the pointer comparison in prv_type_latched_unsupported: a
  // future session could be allocated at the address of the freed one.
  for (uint8_t type = 0; type < ARRAY_LENGTH(s_rx); type++) {
    if (s_rx[type].active) {
      prv_drop(type, s_rx[type].token, s_rx[type].total_bytes, "session closed");
    } else {
      prv_rx_reset(type);
    }
  }
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_latched_session = nullptr;
  s_unsupported_types = 0;
  memset(s_req, 0, sizeof(s_req));
  pbl_mutex_unlock(&s_lock);
}
