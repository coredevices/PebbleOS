/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/imaging.h"

#include "applib/graphics/gtypes.h"
#include "flash_region/flash_region.h"
#include "kernel/pbl_malloc.h"
#include "pbl/kernel/mutex.h"
#include "pbl/services/comm_session/session.h"
#include "pbl/util/math.h"
#include "pbl/util/size.h"
#include "system/status_codes.h"
#include <pbl/drivers/flash.h>
#include <pbl/logging/logging.h>

#include <inttypes.h>
#include <string.h>

static const uint16_t IMAGING_ENDPOINT = 0x35;

// Full-screen 4-bpp on the largest supported display (260x260) is ~34 KB. Cap generously and reject
// anything larger to bound buffer use against a malformed or hostile phone.
#define IMAGING_MAX_BYTES (40 * 1024)
#define IMAGING_MAX_DIM (300)
#define IMAGING_PALETTE_ENTRIES (16)

static ImagingReceivedHandler s_handlers[ImagingImageTypeCount];

//! Guards the latch state and the slot pool below: requests come in on the requesting task (e.g.
//! the Music app) while responses are handled on KernelMain. The reassembly state (s_rx) is
//! deliberately not covered — it is only ever touched on KernelMain (endpoint receiver and
//! comm-session events).
static PBL_MUTEX_DEFINE(s_lock);

// Image types the phone told us it can't serve (ImagingResponseFlagUnsupported), so we stop asking.
// Latched per session: the connected phone doesn't change what it supports mid-connection, and a
// reconnect (possibly to a different phone) clears it via prv_session_types.
static uint32_t s_unsupported_types;
static CommSession *s_latched_session;

static struct {
  bool active;
  uint8_t token;
  GBitmapFormat format;
  uint16_t width;
  uint16_t height;
  uint16_t row_size_bytes;
  uint32_t total_bytes;
  uint32_t received_bytes;
  bool flash;   //!< Pixels go to flash slot `slot`; `pixels` stays NULL.
  int slot;
  uint8_t *pixels;
  GColor *palette;  // NULL for non-palette formats
} s_rx;

// Image store: two fixed slots in the dedicated, memory-mapped IMAGING flash region. Each slot
// holds one decoded bitmap, written as it streams in and rendered straight from flash — no
// pixel-sized heap buffer. Slots are assigned to whichever image types are displaying (not wired to
// a specific consumer), so any two consumers can hold an image at once; a third uses the heap.
//
// A slot lives only while its owner displays it — a display store, not a history cache (the
// consumers already dedupe their own fetches), and imaging is the region's only writer, so stored
// pixels need no integrity check. All access goes through the helpers below, which stub out where
// the region isn't carved so the request/reassembly paths stay free of conditionals.
#ifdef FLASH_REGION_IMAGING_0_BEGIN
#define IMAGING_FLASH_SLOTS 1
#define IMAGING_NUM_SLOTS (2)
_Static_assert(FLASH_REGION_IMAGING_0_END - FLASH_REGION_IMAGING_0_BEGIN >= IMAGING_MAX_BYTES &&
               FLASH_REGION_IMAGING_1_END - FLASH_REGION_IMAGING_1_BEGIN >= IMAGING_MAX_BYTES,
               "each imaging slot must fit any accepted image");

typedef enum {
  SlotIdle,      //!< Not in a load pipeline (blank, or holding a delivered image if `live`).
  SlotErasing,   //!< Erase in flight ahead of a transfer.
  SlotReady,     //!< Blank, waiting for the transfer's first chunk.
  SlotWriting,   //!< The transfer behind `load_token` is streaming in.
} SlotState;

static struct ImagingSlot {
  uint32_t addr;
  uint32_t size;
  SlotState state;
  uint8_t load_token;   //!< Request token being loaded (Erasing/Ready/Writing).
  uint8_t owner;        //!< ImagingImageType displaying this slot (valid while `live`).
  bool live;            //!< A consumer is displaying this slot's image; don't reuse it.
  bool cancel_pending;  //!< The reservation was dropped mid-erase; go Idle when the erase finishes.
} s_slots[IMAGING_NUM_SLOTS] = {
  { .addr = FLASH_REGION_IMAGING_0_BEGIN, .size = FLASH_REGION_IMAGING_0_END -
                                                  FLASH_REGION_IMAGING_0_BEGIN },
  { .addr = FLASH_REGION_IMAGING_1_BEGIN, .size = FLASH_REGION_IMAGING_1_END -
                                                  FLASH_REGION_IMAGING_1_BEGIN },
};

static void prv_slot_erase_done(void *context, status_t result) {
  const int i = (int)(uintptr_t)context;
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_slots[i].state == SlotErasing) {
    // A slot dropped mid-erase (its transfer went to the heap) must not become a Ready reservation
    // no one will ever claim — that would orphan it and, once both slots orphan, force every image
    // onto the heap. Take it back to Idle instead.
    s_slots[i].state = (s_slots[i].cancel_pending || !PASSED(result)) ? SlotIdle : SlotReady;
  }
  s_slots[i].cancel_pending = false;
  pbl_mutex_unlock(&s_lock);
}

//! Drop a pipeline slot safely: an in-flight erase can't just be set Idle (a fresh reserve could
//! pick the slot and race the erase's completion), so mark it for reclaim when the erase lands;
//! anything else is reusable immediately.
static void prv_slot_drop(int i) {
  if (s_slots[i].state == SlotErasing) {
    s_slots[i].cancel_pending = true;
  } else {
    s_slots[i].state = SlotIdle;
  }
}

//! Reserve a slot for the transfer behind `token` and start erasing it, so it is blank by the time
//! the phone's first chunk arrives. Prefers a slot nobody is displaying (leaving the requester's
//! current image up until the new one is ready); failing that reuses the requester's own slot in
//! place; failing that (both slots busy with other consumers) leaves nothing reserved, and the
//! response falls back to the heap.
static void prv_slot_reserve(ImagingImageType type, uint8_t token) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  int target = -1;
  for (int i = 0; i < IMAGING_NUM_SLOTS; ++i) {
    if (s_slots[i].state == SlotIdle && !s_slots[i].live) {
      target = i;
      break;
    }
  }
  if (target < 0) {
    for (int i = 0; i < IMAGING_NUM_SLOTS; ++i) {
      if (s_slots[i].state == SlotIdle && s_slots[i].live && s_slots[i].owner == type) {
        target = i;
        break;
      }
    }
  }
  if (target < 0) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  s_slots[target].state = SlotErasing;
  s_slots[target].load_token = token;
  const uint32_t addr = s_slots[target].addr;
  const uint32_t size = s_slots[target].size;
  pbl_mutex_unlock(&s_lock);
  flash_erase_optimal_range(addr, addr, addr + size, addr + size, prv_slot_erase_done,
                            (void *)(uintptr_t)target);
}

//! Claim the reserved-and-erased slot for transfer `token`. Returns the slot, or -1 to buffer on
//! the heap (no slot was free when the request went out, or the erase hasn't finished). On a miss,
//! drops this token's reservation so a slot whose erase lost the race to the response doesn't
//! orphan.
static int prv_slot_claim(uint8_t token) {
  int slot = -1;
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  for (int i = 0; i < IMAGING_NUM_SLOTS; ++i) {
    if (s_slots[i].load_token != token || s_slots[i].state == SlotIdle) {
      continue;
    }
    if (s_slots[i].state == SlotReady) {
      s_slots[i].state = SlotWriting;
      slot = i;
    } else {
      prv_slot_drop(i);  // reserved but not ready in time; this transfer takes the heap
    }
    break;
  }
  pbl_mutex_unlock(&s_lock);
  return slot;
}

//! Write one chunk into a claimed slot. False if a newer request has reclaimed it, meaning this
//! transfer is dead and the caller should abandon it.
static bool prv_slot_write(int slot, uint32_t offset, const uint8_t *data, uint16_t len,
                           uint8_t token) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool ours = (s_slots[slot].state == SlotWriting && s_slots[slot].load_token == token);
  const uint32_t addr = s_slots[slot].addr;
  pbl_mutex_unlock(&s_lock);
  if (!ours) {
    return false;
  }
  flash_write_bytes(data, addr + offset, len);
  return true;
}

//! Finish a slot load: hand `type`'s display over to it, release whichever slot `type` was showing
//! before (flicker-free when the two differ), and return the memory-mapped pixel pointer.
static const void *prv_slot_commit(int slot, uint8_t type) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_slots[slot].state = SlotIdle;
  s_slots[slot].owner = type;
  for (int j = 0; j < IMAGING_NUM_SLOTS; ++j) {
    if (j != slot && s_slots[j].state == SlotIdle && s_slots[j].live && s_slots[j].owner == type) {
      s_slots[j].live = false;
    }
  }
  s_slots[slot].live = true;
  const void *addr = flash_memory_mapped_address(s_slots[slot].addr);
  pbl_mutex_unlock(&s_lock);
  return addr;
}

//! Abandon the slot reserved for transfer `token` when the request resolves without filling it
//! (the phone had no image, the type is unsupported, or the response was malformed). Leaves any
//! already-delivered image (an Idle+live slot) untouched.
static void prv_slot_cancel(uint8_t token) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  for (int i = 0; i < IMAGING_NUM_SLOTS; ++i) {
    if (s_slots[i].state != SlotIdle && s_slots[i].load_token == token) {
      prv_slot_drop(i);
    }
  }
  pbl_mutex_unlock(&s_lock);
}

//! Drop `type`'s claim on its slot: called when a consumer stops displaying its image outright
//! (rather than replacing it), so the slot can be reused.
static void prv_slot_release(uint8_t type) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  for (int i = 0; i < IMAGING_NUM_SLOTS; ++i) {
    if (s_slots[i].state == SlotIdle && s_slots[i].live && s_slots[i].owner == type) {
      s_slots[i].live = false;
    }
  }
  pbl_mutex_unlock(&s_lock);
}

//! Abandon any in-flight load (e.g. on disconnect); delivered images stay displayable.
static void prv_slots_abandon_loads(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  for (int i = 0; i < IMAGING_NUM_SLOTS; ++i) {
    if (s_slots[i].state != SlotIdle) {
      prv_slot_drop(i);
    }
  }
  pbl_mutex_unlock(&s_lock);
}
#else  // No IMAGING flash region: everything runs through the heap path.
static void prv_slot_reserve(ImagingImageType type, uint8_t token) {}
static int prv_slot_claim(uint8_t token) { return -1; }
static bool prv_slot_write(int slot, uint32_t offset, const uint8_t *data, uint16_t len,
                           uint8_t token) { return false; }
static const void *prv_slot_commit(int slot, uint8_t type) { return NULL; }
static void prv_slot_cancel(uint8_t token) {}
static void prv_slot_release(uint8_t type) {}
static void prv_slots_abandon_loads(void) {}
#endif  // FLASH_REGION_IMAGING_0_BEGIN

static void prv_rx_reset(void) {
  kernel_free(s_rx.pixels);
  kernel_free(s_rx.palette);
  s_rx = (__typeof__(s_rx)) { 0 };
}

void imaging_register_handler(ImagingImageType image_type, ImagingReceivedHandler handler) {
  if (image_type < ARRAY_LENGTH(s_handlers)) {
    s_handlers[image_type] = handler;
  }
}

void imaging_release(ImagingImageType image_type) {
  prv_slot_release(image_type);
}

//! The type a response answers, from the top nibble of its flags byte. Zero — which is what a phone
//! that doesn't set those bits sends — is album art. A value we don't know is left out of range so
//! it routes nowhere rather than to the wrong consumer.
static uint8_t prv_response_type(const ImagingResponseHeader *hdr) {
  return (hdr->flags & IMAGING_RESPONSE_FLAG_TYPE_MASK) >> IMAGING_RESPONSE_FLAG_TYPE_SHIFT;
}

static void prv_deliver(uint8_t token, uint8_t type, GBitmap *bitmap) {
  ImagingReceivedHandler handler = (type < ARRAY_LENGTH(s_handlers)) ? s_handlers[type] : NULL;
  if (handler) {
    handler(token, bitmap);
  } else if (bitmap) {
    if (bitmap->info.is_bitmap_heap_allocated) {
      kernel_free(bitmap->addr);
    }
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
  if (!session ||
      !comm_session_has_capability(session, CommSessionImagingSupport)) {
    return false;
  }
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool latched = prv_type_latched_unsupported(session, image_type);
  pbl_mutex_unlock(&s_lock);
  return !latched;
}

//! Reserve a slot (if any) then send `payload`; shared tail of the two request builders.
static bool prv_send_request(ImagingImageType type, uint8_t token, const uint8_t *payload,
                             size_t length) {
  CommSession *session = comm_session_get_system_session();
  if (!session) {
    return false;
  }
  prv_slot_reserve(type, token);
  return comm_session_send_data(session, IMAGING_ENDPOINT, payload, length,
                                COMM_SESSION_DEFAULT_TIMEOUT);
}

bool imaging_request_album_art(uint8_t token, ImagingFormat format, uint16_t width, uint16_t height,
                               const char *title, const char *artist) {
  if (!imaging_is_type_supported(ImagingImageTypeAlbumArt)) {
    return false;
  }
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

  return prv_send_request(ImagingImageTypeAlbumArt, token, payload, cursor - payload);
}

bool imaging_request_notification_image(uint8_t token, ImagingFormat format, uint16_t width,
                                        uint16_t height, const Uuid *item_id) {
  if (!item_id || !imaging_is_type_supported(ImagingImageTypeNotification)) {
    return false;
  }
  uint8_t payload[sizeof(ImagingRequestHeader) + UUID_SIZE];
  ImagingRequestHeader *hdr = (ImagingRequestHeader *)payload;
  hdr->cmd = ImagingCmdIDRequest;
  hdr->token = token;
  hdr->image_type = ImagingImageTypeNotification;
  hdr->format = format;
  hdr->width = width;
  hdr->height = height;
  memcpy(payload + sizeof(*hdr), item_id, UUID_SIZE);

  return prv_send_request(ImagingImageTypeNotification, token, payload, sizeof(payload));
}

static uint16_t prv_gbitmap_format_for(ImagingFormat format, GBitmapFormat *out) {
  switch (format) {
    case ImagingFormat8BitColor:
      *out = GBitmapFormat8Bit;
      return 0;  // no palette
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
    prv_slot_cancel(hdr->token);
    prv_rx_reset();
    prv_deliver(hdr->token, type, NULL);
    return;
  }

  if (hdr->flags & ImagingResponseFlagNoImage) {
    prv_slot_cancel(hdr->token);
    prv_rx_reset();
    prv_deliver(hdr->token, type, NULL);
    return;
  }

  if (hdr->flags & ImagingResponseFlagFirst) {
    prv_rx_reset();
    if (cursor + 6 > msg_end) {
      PBL_LOG_WRN("Imaging: truncated image header (%zu B)", length);
      prv_slot_cancel(hdr->token);
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
      PBL_LOG_WRN("Imaging: bad image header %ux%u fmt %u palette %u", width, height, format,
                  palette_count);
      prv_slot_cancel(hdr->token);
      return;
    }
    if (cursor + palette_count > msg_end) {
      PBL_LOG_WRN("Imaging: truncated palette (%u entries)", palette_count);
      prv_slot_cancel(hdr->token);
      return;
    }
    const uint16_t row_size = gbitmap_format_get_row_size_bytes(width, gformat);
    const uint32_t total = (uint32_t)row_size * height;
    if (total == 0 || total > IMAGING_MAX_BYTES) {
      PBL_LOG_WRN("Imaging: %ux%u image needs %"PRIu32" B, over the %u B cap", width, height, total,
                  IMAGING_MAX_BYTES);
      prv_slot_cancel(hdr->token);
      return;
    }
    const int slot = prv_slot_claim(hdr->token);
    if (slot >= 0) {
      s_rx.flash = true;
      s_rx.slot = slot;
    } else {
      // INFO, not DBG: a fresh image that can't get a slot is the notable case, and it fires at
      // most once per image so it isn't spammy.
      PBL_LOG_INFO("Imaging: no flash slot free, buffering type %u image on the heap", type);
      s_rx.pixels = kernel_zalloc(total);
      if (!s_rx.pixels) {
        PBL_LOG_WRN("Imaging: out of memory for %ux%u image (%"PRIu32" B)", width, height, total);
        prv_rx_reset();
        prv_deliver(hdr->token, type, NULL);
        return;
      }
    }
    if (max_palette > 0) {
      s_rx.palette = kernel_zalloc(IMAGING_PALETTE_ENTRIES * sizeof(GColor));
      if (!s_rx.palette) {
        PBL_LOG_WRN("Imaging: out of memory for image palette");
        prv_rx_reset();
        prv_deliver(hdr->token, type, NULL);
        return;
      }
      for (uint8_t i = 0; i < palette_count; ++i) {
        s_rx.palette[i] = (GColor) { .argb = cursor[i] };
      }
      cursor += palette_count;
    }
    s_rx.active = true;
    s_rx.token = hdr->token;
    s_rx.format = gformat;
    s_rx.width = width;
    s_rx.height = height;
    s_rx.row_size_bytes = row_size;
    s_rx.total_bytes = total;
    s_rx.received_bytes = 0;
  }

  if (!s_rx.active || s_rx.token != hdr->token) {
    // ponytail: one reassembly slot, so two consumers fetching at once costs one of them a retry.
    // Add a per-token slot array if that ever matters.
    prv_rx_reset();
    return;
  }

  // Reliable, ordered transport: require contiguous in-order chunks with an exact declared length.
  const size_t avail = (cursor <= msg_end) ? (size_t)(msg_end - cursor) : 0;
  if (hdr->offset != s_rx.received_bytes || hdr->chunk_len != avail ||
      (uint32_t)hdr->offset + hdr->chunk_len > s_rx.total_bytes) {
    PBL_LOG_WRN("Imaging: bad chunk at %"PRIu32" (len %u, avail %zu, have %"PRIu32"/%"PRIu32")",
                hdr->offset, hdr->chunk_len, avail, s_rx.received_bytes, s_rx.total_bytes);
    prv_slot_cancel(hdr->token);
    prv_rx_reset();
    return;
  }
  if (s_rx.flash) {
    if (!prv_slot_write(s_rx.slot, hdr->offset, cursor, hdr->chunk_len, hdr->token)) {
      // A newer request reclaimed the slot; this transfer is dead.
      prv_rx_reset();
      return;
    }
  } else {
    memcpy(s_rx.pixels + hdr->offset, cursor, hdr->chunk_len);
  }
  s_rx.received_bytes += hdr->chunk_len;

  if (hdr->flags & ImagingResponseFlagLast) {
    if (s_rx.received_bytes != s_rx.total_bytes) {
      PBL_LOG_WRN("Imaging: short image, got %"PRIu32" of %"PRIu32" B", s_rx.received_bytes,
                  s_rx.total_bytes);
      prv_slot_cancel(hdr->token);
      prv_rx_reset();
      return;
    }
    GBitmap *bmp = kernel_zalloc(sizeof(GBitmap));
    if (!bmp) {
      PBL_LOG_WRN("Imaging: out of memory for GBitmap");
      prv_rx_reset();
      prv_deliver(hdr->token, type, NULL);
      return;
    }
    if (s_rx.flash) {
      bmp->addr = (void *)prv_slot_commit(s_rx.slot, type);
    } else {
      bmp->addr = s_rx.pixels;
      bmp->info.is_bitmap_heap_allocated = true;
    }
    bmp->row_size_bytes = s_rx.row_size_bytes;
    bmp->info.format = s_rx.format;
    bmp->info.version = GBITMAP_VERSION_CURRENT;
    bmp->bounds = (GRect) { { 0, 0 }, { s_rx.width, s_rx.height } };
    bmp->palette = s_rx.palette;
    bmp->info.is_palette_heap_allocated = (s_rx.palette != NULL);
    // Ownership of the buffers moves into the bitmap (slot pixels stay put, flagged not-heap).
    const uint8_t token = s_rx.token;
    s_rx.pixels = NULL;
    s_rx.palette = NULL;
    prv_rx_reset();
    prv_deliver(token, type, bmp);
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
  // future session could be allocated at the address of the freed one. Delivered slot images
  // survive — they stay displayable while disconnected — but abandon any in-flight load.
  prv_rx_reset();
  prv_slots_abandon_loads();
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_latched_session = NULL;
  s_unsupported_types = 0;
  pbl_mutex_unlock(&s_lock);
}
