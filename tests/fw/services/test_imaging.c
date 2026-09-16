/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/imaging.h"

#include "applib/graphics/gtypes.h"
#include "pbl/services/comm_session/session.h"

#include <stdlib.h>
#include <string.h>

// Stubs & Fakes
///////////////////////////////////////////////////////////

#include "fake_session.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"

#include "flash_region/flash_region.h"
#include <pbl/drivers/flash.h>

#include "stubs_bt_lock.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"

// imaging.c only needs the row-size rules from the graphics code; provide them here so the test
// controls the expected sizes without pulling in the renderer.
uint16_t gbitmap_format_get_row_size_bytes(int16_t width, GBitmapFormat format) {
  switch (format) {
    case GBitmapFormat1Bit:
      return ((width + 31) / 32) * 4;
    case GBitmapFormat8Bit:
      return width;
    case GBitmapFormat4BitPalette:
      return (width * 4 + 7) / 8;
    case GBitmapFormat2BitPalette:
      return (width * 2 + 7) / 8;
    case GBitmapFormat1BitPalette:
      return (width + 7) / 8;
    default:
      return 0;
  }
}

// The slot erase completes synchronously against the fake flash, so by the time a request returns
// the slot is ready and the transfer takes the flash path. Tests that want the heap fallback
// receive a response without a matching request first (no slot reserved).
//
// Set s_defer_erase to model real hardware's async erase: the erase is performed but its completion
// callback is withheld until prv_fire_deferred_erase(), so a response can arrive while the slot is
// still "erasing" (the race that used to orphan a slot).
static bool s_defer_erase;
#define MAX_DEFERRED_ERASES (4)
static FlashOperationCompleteCb s_deferred_cb[MAX_DEFERRED_ERASES];
static void *s_deferred_ctx[MAX_DEFERRED_ERASES];
static int s_deferred_count;

void flash_erase_optimal_range(uint32_t min_start, uint32_t max_start, uint32_t min_end,
                               uint32_t max_end, FlashOperationCompleteCb on_complete,
                               void *context) {
  for (uint32_t addr = min_start; addr < max_end; addr += SUBSECTOR_SIZE_BYTES) {
    flash_erase_subsector_blocking(addr);
  }
  if (s_defer_erase) {
    cl_assert(s_deferred_count < MAX_DEFERRED_ERASES);
    s_deferred_cb[s_deferred_count] = on_complete;
    s_deferred_ctx[s_deferred_count] = context;
    s_deferred_count++;
  } else {
    on_complete(context, S_SUCCESS);
  }
}

static void prv_fire_deferred_erases(void) {
  const int n = s_deferred_count;
  s_deferred_count = 0;
  for (int i = 0; i < n; ++i) {
    s_deferred_cb[i](s_deferred_ctx[i], S_SUCCESS);
  }
}

// Delivery capture
///////////////////////////////////////////////////////////

static int s_deliveries;
static uint8_t s_last_token;
static GBitmap *s_last_bitmap;

static void prv_free_last_bitmap(void) {
  if (s_last_bitmap) {
    if (s_last_bitmap->info.is_bitmap_heap_allocated) {
      kernel_free(s_last_bitmap->addr);
    }
    kernel_free(s_last_bitmap->palette);
    kernel_free(s_last_bitmap);
    s_last_bitmap = NULL;
  }
}

static void prv_art_handler(uint8_t token, GBitmap *bitmap) {
  s_deliveries++;
  s_last_token = token;
  prv_free_last_bitmap();
  s_last_bitmap = bitmap;
}

static int s_notif_deliveries;

static void prv_notif_handler(uint8_t token, GBitmap *bitmap) {
  s_notif_deliveries++;
  prv_free_last_bitmap();
  s_last_bitmap = bitmap;
}

static uint8_t prv_typed(ImagingImageType type, uint8_t flags) {
  return flags | (type << IMAGING_RESPONSE_FLAG_TYPE_SHIFT);
}

// Helpers
///////////////////////////////////////////////////////////

#define TEST_TOKEN (42)

//! Build an ImageResponse into `out`: header, then (on First) the image header + palette, then
//! `pixel_len` pixel bytes. `chunk_len_field` is what goes on the wire, which tests may set to a
//! lie; pass the real pixel count for well-formed messages.
static size_t prv_build_response(uint8_t *out, uint8_t token, uint8_t flags, uint32_t offset,
                                 uint16_t chunk_len_field, uint16_t width, uint16_t height,
                                 uint8_t format, const uint8_t *palette, uint8_t palette_count,
                                 const uint8_t *pixels, size_t pixel_len) {
  ImagingResponseHeader *hdr = (ImagingResponseHeader *)out;
  *hdr = (ImagingResponseHeader) {
    .cmd = ImagingCmdIDResponse,
    .token = token,
    .flags = flags,
    .offset = offset,
    .chunk_len = chunk_len_field,
  };
  uint8_t *cursor = out + sizeof(*hdr);
  if (flags & ImagingResponseFlagFirst) {
    *cursor++ = (uint8_t)(width & 0xff);
    *cursor++ = (uint8_t)(width >> 8);
    *cursor++ = (uint8_t)(height & 0xff);
    *cursor++ = (uint8_t)(height >> 8);
    *cursor++ = format;
    *cursor++ = palette_count;
    memcpy(cursor, palette, palette_count);
    cursor += palette_count;
  }
  memcpy(cursor, pixels, pixel_len);
  cursor += pixel_len;
  return cursor - out;
}

static void prv_receive(const uint8_t *msg, size_t length) {
  imaging_protocol_msg_callback(NULL, msg, length);
}

//! A well-formed 4x2 4-bpp image: row size 2, total 4 pixel bytes, 3 palette entries.
static const uint8_t s_palette[] = { 0xC0, 0xF0, 0xFF };
static const uint8_t s_pixels[] = { 0x01, 0x20, 0x12, 0x01 };

static void prv_receive_valid_image(uint8_t token) {
  uint8_t buf[64];
  const size_t len = prv_build_response(
      buf, token, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, sizeof(s_pixels),
      4, 2, ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, sizeof(s_pixels));
  prv_receive(buf, len);
}

// Tests
///////////////////////////////////////////////////////////

static Transport *s_transport;

// The two image slots live in the IMAGING flash region; back it with fake flash. The two slots are
// non-contiguous (carved from separate RSVD areas), so init a window spanning both.
void test_imaging__initialize(void) {
  s_defer_erase = false;
  s_deferred_count = 0;
  fake_spi_flash_init(FLASH_REGION_IMAGING_0_BEGIN,
                      FLASH_REGION_IMAGING_1_END - FLASH_REGION_IMAGING_0_BEGIN);
  fake_comm_session_init();
  s_transport = fake_transport_create(TransportDestinationSystem, NULL, NULL);
  fake_transport_set_connected(s_transport, true);
  imaging_register_handler(ImagingImageTypeAlbumArt, prv_art_handler);
  imaging_register_handler(ImagingImageTypeNotification, prv_notif_handler);
  s_deliveries = 0;
  s_notif_deliveries = 0;
  s_last_token = 0;
  s_last_bitmap = NULL;
  // The slot pool is static and persists across tests: abandon any in-flight load (session close)
  // and release both types' held slots so every test starts with an empty pool.
  const PebbleCommSessionEvent closed_event = {
    .is_open = false,
    .is_system = true,
  };
  imaging_handle_comm_session_event(&closed_event);
  imaging_release(ImagingImageTypeAlbumArt);
  imaging_release(ImagingImageTypeNotification);
}

void test_imaging__cleanup(void) {
  prv_free_last_bitmap();
  fake_comm_session_cleanup();
  fake_spi_flash_cleanup();
}

void test_imaging__single_chunk_image(void) {
  prv_receive_valid_image(TEST_TOKEN);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert_equal_i(s_last_token, TEST_TOKEN);
  cl_assert(s_last_bitmap != NULL);
  cl_assert_equal_i(s_last_bitmap->bounds.size.w, 4);
  cl_assert_equal_i(s_last_bitmap->bounds.size.h, 2);
  cl_assert_equal_i(s_last_bitmap->row_size_bytes, 2);
  cl_assert_equal_i(s_last_bitmap->info.format, GBitmapFormat4BitPalette);
  cl_assert(memcmp(s_last_bitmap->addr, s_pixels, sizeof(s_pixels)) == 0);
  cl_assert_equal_i(((GColor *)s_last_bitmap->palette)[1].argb, s_palette[1]);
}

void test_imaging__multi_chunk_image(void) {
  uint8_t buf[64];
  size_t len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst, 0, 2,
                                  4, 2, ImagingFormat4BitPalette,
                                  s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagLast, 2, 2,
                           0, 0, 0, NULL, 0, s_pixels + 2, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(s_last_bitmap != NULL);
  cl_assert(memcmp(s_last_bitmap->addr, s_pixels, sizeof(s_pixels)) == 0);
}

void test_imaging__no_image(void) {
  uint8_t buf[32];
  const size_t len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagNoImage, 0, 0,
                                        0, 0, 0, NULL, 0, NULL, 0);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(s_last_bitmap == NULL);
}

void test_imaging__truncated_header_rejected(void) {
  uint8_t buf[64];
  const size_t len = prv_build_response(
      buf, TEST_TOKEN, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, sizeof(s_pixels),
      4, 2, ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, sizeof(s_pixels));
  // Truncate inside the image header, inside the palette, and inside the response header
  prv_receive(buf, sizeof(ImagingResponseHeader) + 3);
  prv_receive(buf, sizeof(ImagingResponseHeader) + 6 + 1);
  prv_receive(buf, sizeof(ImagingResponseHeader) - 1);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__bad_dimensions_rejected(void) {
  uint8_t buf[64];
  // Width over the cap
  size_t len = prv_build_response(buf, TEST_TOKEN,
                                  ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, 4,
                                  301, 2, ImagingFormat4BitPalette,
                                  s_palette, sizeof(s_palette), s_pixels, 4);
  prv_receive(buf, len);
  // Zero height
  len = prv_build_response(buf, TEST_TOKEN,
                           ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, 4,
                           4, 0, ImagingFormat4BitPalette,
                           s_palette, sizeof(s_palette), s_pixels, 4);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__bad_palette_rejected(void) {
  uint8_t big_palette[17] = { 0 };
  uint8_t buf[64];
  // More palette entries than a 4-bpp image can have
  size_t len = prv_build_response(buf, TEST_TOKEN,
                                  ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, 4,
                                  4, 2, ImagingFormat4BitPalette,
                                  big_palette, sizeof(big_palette), s_pixels, 4);
  prv_receive(buf, len);
  // A palettized format with no palette at all
  len = prv_build_response(buf, TEST_TOKEN,
                           ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, 4,
                           4, 2, ImagingFormat4BitPalette, NULL, 0, s_pixels, 4);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__oversized_image_rejected(void) {
  uint8_t buf[64];
  // 300x300 8-bit = 90000 bytes, over IMAGING_MAX_BYTES
  const size_t len = prv_build_response(buf, TEST_TOKEN,
                                        ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, 4,
                                        300, 300, ImagingFormat8BitColor, NULL, 0, s_pixels, 4);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__non_contiguous_chunk_resets(void) {
  uint8_t buf[64];
  size_t len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst, 0, 2,
                                  4, 2, ImagingFormat4BitPalette,
                                  s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  // Wrong offset: skips a byte
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagLast, 3, 1,
                           0, 0, 0, NULL, 0, s_pixels + 3, 1);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
  // The transfer was reset: a well-formed follow-up chunk must also be ignored
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagLast, 2, 2,
                           0, 0, 0, NULL, 0, s_pixels + 2, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__lying_chunk_len_resets(void) {
  uint8_t buf[64];
  size_t len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst, 0, 2,
                                  4, 2, ImagingFormat4BitPalette,
                                  s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  // chunk_len claims more pixel bytes than the message carries
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagLast, 2, 60,
                           0, 0, 0, NULL, 0, s_pixels + 2, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__incomplete_transfer_not_delivered(void) {
  uint8_t buf[64];
  // Last chunk arrives before all pixel bytes were received
  const size_t len = prv_build_response(buf, TEST_TOKEN,
                                        ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, 2,
                                        4, 2, ImagingFormat4BitPalette,
                                        s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__token_mismatch_resets(void) {
  uint8_t buf[64];
  size_t len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst, 0, 2,
                                  4, 2, ImagingFormat4BitPalette,
                                  s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  // Continuation with a different token must not complete the transfer
  len = prv_build_response(buf, TEST_TOKEN + 1, ImagingResponseFlagLast, 2, 2,
                           0, 0, 0, NULL, 0, s_pixels + 2, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__unsupported_latches_until_session_close(void) {
  cl_assert(imaging_is_type_supported(ImagingImageTypeAlbumArt));

  uint8_t buf[32];
  const size_t len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagUnsupported, 0, 0,
                                        0, 0, 0, NULL, 0, NULL, 0);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(s_last_bitmap == NULL);
  cl_assert(!imaging_is_type_supported(ImagingImageTypeAlbumArt));
  // Latching is per type
  cl_assert(imaging_is_type_supported(ImagingImageTypeNotification));

  // Closing the system session clears the latch
  const PebbleCommSessionEvent closed_event = {
    .is_open = false,
    .is_system = true,
  };
  imaging_handle_comm_session_event(&closed_event);
  cl_assert(imaging_is_type_supported(ImagingImageTypeAlbumArt));
}

void test_imaging__request_payload_format(void) {
  cl_assert(imaging_request_album_art(7, ImagingFormat4BitPalette, 166, 166, "Title", "Artist"));
  fake_comm_session_process_send_next();
  const uint8_t expected[] = {
    0x01, 7, 0x00, 0x02, 166, 0, 166, 0,
    5, 'T', 'i', 't', 'l', 'e',
    6, 'A', 'r', 't', 'i', 's', 't',
  };
  fake_transport_assert_sent(s_transport, 0, 0x35, expected, sizeof(expected));
}

void test_imaging__notification_request_payload_format(void) {
  const Uuid id = UuidMake(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                           0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10);
  cl_assert(imaging_request_notification_image(9, ImagingFormat4BitPalette, 180, 135, &id));
  fake_comm_session_process_send_next();
  const uint8_t expected[] = {
    0x01, 9, 0x01, 0x02, 180, 0, 135, 0,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
  };
  fake_transport_assert_sent(s_transport, 0, 0x35, expected, sizeof(expected));
}

static const Uuid s_notif_id = UuidMake(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

//! The well-formed 4x2 image, typed as a notification image.
static void prv_receive_valid_notification_image(uint8_t token) {
  uint8_t buf[64];
  prv_receive(buf, prv_build_response(
      buf, token,
      prv_typed(ImagingImageTypeNotification,
                ImagingResponseFlagFirst | ImagingResponseFlagLast),
      0, sizeof(s_pixels), 4, 2, ImagingFormat4BitPalette,
      s_palette, sizeof(s_palette), s_pixels, sizeof(s_pixels)));
}

//! True if `addr` points inside either fake-flash imaging slot.
static bool prv_addr_in_slots(const void *addr) {
  const uint8_t *p = addr;
  return (p >= flash_memory_mapped_address(FLASH_REGION_IMAGING_0_BEGIN) &&
          p < flash_memory_mapped_address(FLASH_REGION_IMAGING_0_END - 1)) ||
         (p >= flash_memory_mapped_address(FLASH_REGION_IMAGING_1_BEGIN) &&
          p < flash_memory_mapped_address(FLASH_REGION_IMAGING_1_END - 1));
}

void test_imaging__notification_image_stored_in_slot(void) {
  cl_assert(imaging_request_notification_image(9, ImagingFormat4BitPalette, 180, 136,
                                              &s_notif_id));
  prv_receive_valid_notification_image(9);
  cl_assert_equal_i(s_notif_deliveries, 1);
  cl_assert(s_last_bitmap != NULL);
  cl_assert(!s_last_bitmap->info.is_bitmap_heap_allocated);
  cl_assert(prv_addr_in_slots(s_last_bitmap->addr));
  cl_assert(memcmp(s_last_bitmap->addr, s_pixels, sizeof(s_pixels)) == 0);
  cl_assert_equal_i(((GColor *)s_last_bitmap->palette)[1].argb, s_palette[1]);
}

void test_imaging__heap_fallback_without_reservation(void) {
  // No request first, so no slot was reserved: buffered on the heap.
  prv_receive_valid_notification_image(9);
  cl_assert_equal_i(s_notif_deliveries, 1);
  cl_assert(s_last_bitmap != NULL);
  cl_assert(s_last_bitmap->info.is_bitmap_heap_allocated);
  cl_assert(memcmp(s_last_bitmap->addr, s_pixels, sizeof(s_pixels)) == 0);
}

void test_imaging__two_consumers_hold_slots_at_once(void) {
  // Album art takes one slot...
  cl_assert(imaging_request_album_art(7, ImagingFormat4BitPalette, 166, 166, "Song", "Band"));
  prv_receive_valid_image(7);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(!s_last_bitmap->info.is_bitmap_heap_allocated);
  const void *art_addr = s_last_bitmap->addr;

  // ...a notification image takes the other, concurrently, in a different slot.
  cl_assert(imaging_request_notification_image(9, ImagingFormat4BitPalette, 180, 136,
                                              &s_notif_id));
  prv_receive_valid_notification_image(9);
  cl_assert_equal_i(s_notif_deliveries, 1);
  cl_assert(!s_last_bitmap->info.is_bitmap_heap_allocated);
  cl_assert(prv_addr_in_slots(s_last_bitmap->addr));
  cl_assert(s_last_bitmap->addr != art_addr);
}

void test_imaging__replacing_own_image_stays_flash_backed(void) {
  // Only notifications active: one slot free. First image loads.
  cl_assert(imaging_request_notification_image(9, ImagingFormat4BitPalette, 180, 136,
                                              &s_notif_id));
  prv_receive_valid_notification_image(9);
  cl_assert_equal_i(s_notif_deliveries, 1);

  // A new notification replaces it. With a free slot it loads flicker-free into the other slot;
  // either way it stays flash-backed and delivers.
  const Uuid other = UuidMake(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2);
  cl_assert(imaging_request_notification_image(10, ImagingFormat4BitPalette, 180, 136, &other));
  prv_receive_valid_notification_image(10);
  cl_assert_equal_i(s_notif_deliveries, 2);
  cl_assert(!s_last_bitmap->info.is_bitmap_heap_allocated);
  cl_assert(prv_addr_in_slots(s_last_bitmap->addr));

  // After releasing, the slots are all reusable again.
  imaging_release(ImagingImageTypeNotification);
  cl_assert(imaging_request_notification_image(11, ImagingFormat4BitPalette, 180, 136,
                                              &s_notif_id));
  prv_receive_valid_notification_image(11);
  cl_assert_equal_i(s_notif_deliveries, 3);
  cl_assert(!s_last_bitmap->info.is_bitmap_heap_allocated);
}

void test_imaging__superseded_transfer_is_abandoned(void) {
  cl_assert(imaging_request_notification_image(9, ImagingFormat4BitPalette, 180, 136,
                                              &s_notif_id));
  // Transfer 9 starts streaming into its slot...
  uint8_t buf[64];
  size_t len = prv_build_response(
      buf, 9, prv_typed(ImagingImageTypeNotification, ImagingResponseFlagFirst), 0, 2,
      4, 2, ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  // ...but album art (a different consumer) fetches and completes into the other slot.
  cl_assert(imaging_request_album_art(7, ImagingFormat4BitPalette, 166, 166, "Song", "Band"));
  prv_receive_valid_image(7);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(!s_last_bitmap->info.is_bitmap_heap_allocated);
  // The abandoned notification transfer resets s_rx; its stale last chunk delivers nothing.
  cl_assert_equal_i(s_notif_deliveries, 0);
}

void test_imaging__no_image_frees_the_reserved_slot(void) {
  // Reserve a slot, then the phone reports no image: the slot must be freed, not leaked.
  cl_assert(imaging_request_notification_image(9, ImagingFormat4BitPalette, 180, 136,
                                              &s_notif_id));
  uint8_t buf[32];
  prv_receive(buf, prv_build_response(
      buf, 9, prv_typed(ImagingImageTypeNotification, ImagingResponseFlagNoImage),
      0, 0, 0, 0, 0, NULL, 0, NULL, 0));
  cl_assert_equal_i(s_notif_deliveries, 1);
  cl_assert(s_last_bitmap == NULL);

  // Both slots are free again, so two fresh images both land in flash (not the heap).
  cl_assert(imaging_request_album_art(7, ImagingFormat4BitPalette, 166, 166, "Song", "Band"));
  prv_receive_valid_image(7);
  cl_assert(!s_last_bitmap->info.is_bitmap_heap_allocated);
  cl_assert(imaging_request_notification_image(10, ImagingFormat4BitPalette, 180, 136,
                                              &s_notif_id));
  prv_receive_valid_notification_image(10);
  cl_assert(!s_last_bitmap->info.is_bitmap_heap_allocated);
}

void test_imaging__erase_race_does_not_orphan_slots(void) {
  // Model real hardware: the erase is async, so a fast response can arrive before the slot is
  // erased. Both slots race and both images fall back to the heap.
  s_defer_erase = true;
  cl_assert(imaging_request_notification_image(9, ImagingFormat4BitPalette, 180, 136,
                                              &s_notif_id));
  prv_receive_valid_notification_image(9);
  cl_assert(s_last_bitmap->info.is_bitmap_heap_allocated);
  cl_assert(imaging_request_album_art(7, ImagingFormat4BitPalette, 166, 166, "Song", "Band"));
  prv_receive_valid_image(7);
  cl_assert(s_last_bitmap->info.is_bitmap_heap_allocated);

  // Both erases now complete. The dropped reservations must return to Idle, not become orphaned
  // Ready slots that no response will ever claim (which would force every later image to the heap).
  prv_fire_deferred_erases();

  // With the erase synchronous again, a fresh image must land in a flash slot — proving the two
  // slots were reclaimed rather than leaked.
  s_defer_erase = false;
  cl_assert(imaging_request_notification_image(11, ImagingFormat4BitPalette, 180, 136,
                                              &s_notif_id));
  prv_receive_valid_notification_image(11);
  cl_assert(!s_last_bitmap->info.is_bitmap_heap_allocated);
  cl_assert(prv_addr_in_slots(s_last_bitmap->addr));
}

void test_imaging__response_routed_by_type(void) {
  uint8_t buf[64];
  const size_t len = prv_build_response(
      buf, TEST_TOKEN,
      prv_typed(ImagingImageTypeNotification,
                ImagingResponseFlagFirst | ImagingResponseFlagLast),
      0, sizeof(s_pixels), 4, 2, ImagingFormat4BitPalette,
      s_palette, sizeof(s_palette), s_pixels, sizeof(s_pixels));
  prv_receive(buf, len);
  cl_assert_equal_i(s_notif_deliveries, 1);
  cl_assert_equal_i(s_deliveries, 0);
  cl_assert(s_last_bitmap != NULL);
}

void test_imaging__untyped_response_goes_to_album_art(void) {
  // A phone that doesn't set the type bits only ever serves album art.
  prv_receive_valid_image(TEST_TOKEN);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert_equal_i(s_notif_deliveries, 0);
}

void test_imaging__interleaved_requests_route_correctly(void) {
  const Uuid id = UuidMake(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  cl_assert(imaging_request_album_art(7, ImagingFormat4BitPalette, 166, 166, "Title", "Artist"));
  cl_assert(imaging_request_notification_image(9, ImagingFormat4BitPalette, 180, 135, &id));
  // The album art response arrives after the notification request was sent; it must still reach
  // the album art handler.
  prv_receive_valid_image(7);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert_equal_i(s_notif_deliveries, 0);
}

void test_imaging__unsupported_latches_per_type(void) {
  // A response only ever follows a request, which always checks support first.
  cl_assert(imaging_is_type_supported(ImagingImageTypeNotification));

  uint8_t buf[32];
  const size_t len = prv_build_response(
      buf, TEST_TOKEN, prv_typed(ImagingImageTypeNotification, ImagingResponseFlagUnsupported),
      0, 0, 0, 0, 0, NULL, 0, NULL, 0);
  prv_receive(buf, len);
  cl_assert_equal_i(s_notif_deliveries, 1);
  cl_assert(!imaging_is_type_supported(ImagingImageTypeNotification));
  cl_assert(imaging_is_type_supported(ImagingImageTypeAlbumArt));
}
