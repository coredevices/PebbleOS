/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <string.h>

#include <pbl/services/comm_session/session.h>
#include <pbl/services/imaging.h>
#include <pbl/services/resident_image.h>
#include <pbl/util/heap.h>

#include <applib/graphics/graphics_bitmap.h>
#include <applib/graphics/gtypes.h>
#include <clar.h>
#include <kernel/pbl_malloc.h>
#include <stubs_pbl_malloc.h>

// Stubs & Fakes
///////////////////////////////////////////////////////////

#include <fake_session.h>
#include <fake_system_task.h>

#ifdef CONFIG_IMAGE_CACHE
#include <fake_mutex.h>
#include <fake_psram.h>
#include <kernel/psram_image_pool.h>
#endif

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

// Delivery capture
///////////////////////////////////////////////////////////

static int s_deliveries;
static int s_will_receives;
static int s_failures;
static uint8_t s_last_token;
static uint8_t s_last_failure_token;
static ResidentImage *s_last_image;

// Reads an image back through what it draws
static uint8_t s_drawn[64];

void graphics_draw_bitmap_in_rect(GContext *ctx, const GBitmap *bitmap, const GRect *rect) {
  for (int y = 0; y < rect->size.h; y++) {
    memcpy(&s_drawn[(rect->origin.y + y) * bitmap->row_size_bytes],
           (const uint8_t *)bitmap->addr + y * bitmap->row_size_bytes, bitmap->row_size_bytes);
  }
}

static const uint8_t *prv_image_pixels(ResidentImage *image) {
  memset(s_drawn, 0, sizeof(s_drawn));
  const GSize size = resident_image_get_size(image);
  resident_image_draw(nullptr, image, &GRect(0, 0, size.w, size.h));
  return s_drawn;
}

Heap *kernel_heap_get(void) {
  static Heap heap;
  return &heap;
}

void heap_calc_totals(Heap *heap, unsigned int *used, unsigned int *free_bytes,
                      unsigned int *max_free) {
  *used = 0;
  *free_bytes = 0;
  *max_free = 64 * 1024;
}

static void prv_free_last_image(void) {
  resident_image_destroy(s_last_image);
  s_last_image = nullptr;
}

static void prv_art_handler(uint8_t token, ResidentImage *image) {
  s_deliveries++;
  s_last_token = token;
  prv_free_last_image();
  s_last_image = image;
}

static void prv_will_receive_handler(uint8_t token) {
  s_will_receives++;
}

static void prv_failure_handler(uint8_t token) {
  s_failures++;
  s_last_failure_token = token;
}

static int s_notif_deliveries;

static void prv_notif_handler(uint8_t token, ResidentImage *image) {
  s_notif_deliveries++;
  prv_free_last_image();
  s_last_image = image;
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
  *hdr = (ImagingResponseHeader){
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
  imaging_protocol_msg_callback(nullptr, msg, length);
}

//! A well-formed 4x2 4-bpp image: row size 2, total 4 pixel bytes, 3 palette entries.
static const uint8_t s_palette[] = {0xC0, 0xF0, 0xFF};
static const uint8_t s_pixels[] = {0x01, 0x20, 0x12, 0x01};

static void prv_receive_valid_image(uint8_t token) {
  uint8_t buf[64];
  const size_t len = prv_build_response(
      buf, token, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, sizeof(s_pixels), 4, 2,
      ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, sizeof(s_pixels));
  prv_receive(buf, len);
}

// Tests
///////////////////////////////////////////////////////////

static Transport *s_transport;

void test_imaging__initialize(void) {
  fake_comm_session_init();
  s_transport = fake_transport_create(TransportDestinationSystem, nullptr, nullptr);
  fake_transport_set_connected(s_transport, true);
  imaging_register_handler(ImagingImageTypeAlbumArt, prv_art_handler);
  imaging_register_transfer_handlers(ImagingImageTypeAlbumArt, prv_will_receive_handler,
                                     prv_failure_handler);
  imaging_register_handler(ImagingImageTypeNotification, prv_notif_handler);
  // Reset any in-flight transfer or latched state left over from a previous test.
  const PebbleCommSessionEvent closed_event = {
    .is_open = false,
    .is_system = true,
  };
  imaging_handle_comm_session_event(&closed_event);
  s_deliveries = 0;
  s_will_receives = 0;
  s_failures = 0;
  s_notif_deliveries = 0;
  s_last_token = 0;
  s_last_failure_token = 0;
  s_last_image = nullptr;
  stub_pbl_malloc_set_kernel_malloc_should_fail(false);
}

void test_imaging__cleanup(void) {
  prv_free_last_image();
#ifdef CONFIG_IMAGE_CACHE
  psram_retain_acquire();
  image_cache_flush(ImageCacheClientAlbumArt);
  image_cache_flush(ImageCacheClientNotification);
  psram_retain_release();
  cl_assert_equal_i(psram_image_pool_used(), 0);
  cl_assert_equal_i(fake_psram_holds(), 0);
#endif
  fake_comm_session_cleanup();
}

void test_imaging__single_chunk_image(void) {
  prv_receive_valid_image(TEST_TOKEN);
  cl_assert_equal_i(s_will_receives, 1);
  cl_assert_equal_i(s_failures, 0);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert_equal_i(s_last_token, TEST_TOKEN);
  cl_assert(s_last_image != nullptr);
  cl_assert_equal_i(resident_image_get_size(s_last_image).w, 4);
  cl_assert_equal_i(resident_image_get_size(s_last_image).h, 2);
  cl_assert_equal_i(resident_image_get_data_size(s_last_image), sizeof(s_pixels));
  cl_assert(memcmp(prv_image_pixels(s_last_image), s_pixels, sizeof(s_pixels)) == 0);
  cl_assert_equal_i(resident_image_get_palette(s_last_image)[1].argb, s_palette[1]);
}

void test_imaging__multi_chunk_image(void) {
  uint8_t buf[64];
  size_t len =
      prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst, 0, 2, 4, 2,
                         ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagLast, 2, 2, 0, 0, 0, nullptr, 0,
                           s_pixels + 2, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(s_last_image != nullptr);
  cl_assert(memcmp(prv_image_pixels(s_last_image), s_pixels, sizeof(s_pixels)) == 0);
}

void test_imaging__allocation_failure_notifies_and_resets(void) {
  stub_pbl_malloc_set_kernel_malloc_should_fail(true);
  prv_receive_valid_image(TEST_TOKEN);
  stub_pbl_malloc_set_kernel_malloc_should_fail(false);

  cl_assert_equal_i(s_will_receives, 1);
  cl_assert_equal_i(s_failures, 1);
  cl_assert_equal_i(s_last_failure_token, TEST_TOKEN);
  cl_assert_equal_i(s_deliveries, 0);

  prv_receive_valid_image(TEST_TOKEN + 1);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert_equal_i(s_last_token, TEST_TOKEN + 1);
}

void test_imaging__session_close_mid_transfer_notifies_and_resets(void) {
  uint8_t buf[64];
  const size_t len =
      prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst, 0, 2, 4, 2,
                         ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  const PebbleCommSessionEvent closed_event = {
    .is_open = false,
    .is_system = true,
  };
  imaging_handle_comm_session_event(&closed_event);

  cl_assert_equal_i(s_failures, 1);
  cl_assert_equal_i(s_last_failure_token, TEST_TOKEN);
  cl_assert_equal_i(s_deliveries, 0);

  prv_receive_valid_image(TEST_TOKEN + 1);
  cl_assert_equal_i(s_deliveries, 1);
}

void test_imaging__no_image(void) {
  uint8_t buf[32];
  const size_t len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagNoImage, 0, 0, 0, 0, 0,
                                        nullptr, 0, nullptr, 0);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(s_last_image == nullptr);
}

void test_imaging__truncated_header_rejected(void) {
  uint8_t buf[64];
  prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0,
                     sizeof(s_pixels), 4, 2, ImagingFormat4BitPalette, s_palette, sizeof(s_palette),
                     s_pixels, sizeof(s_pixels));
  // Truncate inside the image header, inside the palette, and inside the response header
  prv_receive(buf, sizeof(ImagingResponseHeader) + 3);
  prv_receive(buf, sizeof(ImagingResponseHeader) + 6 + 1);
  prv_receive(buf, sizeof(ImagingResponseHeader) - 1);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__bad_dimensions_rejected(void) {
  uint8_t buf[64];
  // Width over the cap
  size_t len = prv_build_response(
      buf, TEST_TOKEN, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, 4, 301, 2,
      ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, 4);
  prv_receive(buf, len);
  // Zero height
  len =
      prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, 4,
                         4, 0, ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, 4);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__bad_palette_rejected(void) {
  uint8_t big_palette[17] = {};
  uint8_t buf[64];
  // More palette entries than a 4-bpp image can have
  size_t len = prv_build_response(
      buf, TEST_TOKEN, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, 4, 4, 2,
      ImagingFormat4BitPalette, big_palette, sizeof(big_palette), s_pixels, 4);
  prv_receive(buf, len);
  // A palettized format with no palette at all
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0,
                           4, 4, 2, ImagingFormat4BitPalette, nullptr, 0, s_pixels, 4);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__oversized_image_rejected(void) {
  uint8_t buf[64];
  // 300x300 8-bit = 90000 bytes, over IMAGING_MAX_BYTES
  const size_t len =
      prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, 4,
                         300, 300, ImagingFormat8BitColor, nullptr, 0, s_pixels, 4);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__non_contiguous_chunk_resets(void) {
  uint8_t buf[64];
  size_t len =
      prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst, 0, 2, 4, 2,
                         ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  // Wrong offset: skips a byte
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagLast, 3, 1, 0, 0, 0, nullptr, 0,
                           s_pixels + 3, 1);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
  cl_assert_equal_i(s_failures, 1);
  cl_assert_equal_i(s_last_failure_token, TEST_TOKEN);
  // The transfer was reset: a well-formed follow-up chunk must also be ignored
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagLast, 2, 2, 0, 0, 0, nullptr, 0,
                           s_pixels + 2, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__lying_chunk_len_resets(void) {
  uint8_t buf[64];
  size_t len =
      prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst, 0, 2, 4, 2,
                         ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  // chunk_len claims more pixel bytes than the message carries
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagLast, 2, 60, 0, 0, 0, nullptr, 0,
                           s_pixels + 2, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__incomplete_transfer_not_delivered(void) {
  uint8_t buf[64];
  // Last chunk arrives before all pixel bytes were received
  const size_t len =
      prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, 2,
                         4, 2, ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__token_mismatch_resets(void) {
  uint8_t buf[64];
  size_t len =
      prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst, 0, 2, 4, 2,
                         ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  // Continuation with a different token must not complete the transfer
  len = prv_build_response(buf, TEST_TOKEN + 1, ImagingResponseFlagLast, 2, 2, 0, 0, 0, nullptr, 0,
                           s_pixels + 2, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__unsupported_latches_until_session_close(void) {
  cl_assert(imaging_is_type_supported(ImagingImageTypeAlbumArt));

  uint8_t buf[32];
  const size_t len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagUnsupported, 0, 0, 0, 0,
                                        0, nullptr, 0, nullptr, 0);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(s_last_image == nullptr);
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
  cl_assert(
      imaging_request_album_art(7, ImagingFormat4BitPalette, 166, 166, "Title", "Artist", nullptr));
  fake_comm_session_process_send_next();
  const uint8_t expected[] = {
    0x01, 7,   0x00, 0x02, 166, 0,   166, 0,   5,   'T', 'i',
    't',  'l', 'e',  6,    'A', 'r', 't', 'i', 's', 't',
  };
  fake_transport_assert_sent(s_transport, 0, 0x35, expected, sizeof(expected));
}

void test_imaging__notification_request_payload_format(void) {
  const Uuid id = UuidMake(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
                           0x0d, 0x0e, 0x0f, 0x10);
  cl_assert(imaging_request_notification_image(9, ImagingFormat4BitPalette, 180, 135, &id));
  fake_comm_session_process_send_next();
  const uint8_t expected[] = {
    0x01, 9,    0x01, 0x02, 180,  0,    135,  0,    0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
  };
  fake_transport_assert_sent(s_transport, 0, 0x35, expected, sizeof(expected));
}

void test_imaging__response_routed_by_type(void) {
  uint8_t buf[64];
  const size_t len = prv_build_response(
      buf, TEST_TOKEN,
      prv_typed(ImagingImageTypeNotification, ImagingResponseFlagFirst | ImagingResponseFlagLast),
      0, sizeof(s_pixels), 4, 2, ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels,
      sizeof(s_pixels));
  prv_receive(buf, len);
  cl_assert_equal_i(s_notif_deliveries, 1);
  cl_assert_equal_i(s_deliveries, 0);
  cl_assert(s_last_image != nullptr);
}

void test_imaging__untyped_response_goes_to_album_art(void) {
  // A phone that doesn't set the type bits only ever serves album art.
  prv_receive_valid_image(TEST_TOKEN);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert_equal_i(s_notif_deliveries, 0);
}

void test_imaging__interleaved_requests_route_correctly(void) {
  const Uuid id = UuidMake(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  cl_assert(
      imaging_request_album_art(7, ImagingFormat4BitPalette, 166, 166, "Title", "Artist", nullptr));
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
      buf, TEST_TOKEN, prv_typed(ImagingImageTypeNotification, ImagingResponseFlagUnsupported), 0,
      0, 0, 0, 0, nullptr, 0, nullptr, 0);
  prv_receive(buf, len);
  cl_assert_equal_i(s_notif_deliveries, 1);
  cl_assert(!imaging_is_type_supported(ImagingImageTypeNotification));
  cl_assert(imaging_is_type_supported(ImagingImageTypeAlbumArt));
}

void test_imaging__concurrent_transfers_of_different_types(void) {
  uint8_t buf[64];
  size_t len =
      prv_build_response(buf, 7, ImagingResponseFlagFirst, 0, 2, 4, 2, ImagingFormat4BitPalette,
                         s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  // A notification image arrives in the middle of the album art
  len = prv_build_response(
      buf, 9,
      prv_typed(ImagingImageTypeNotification, ImagingResponseFlagFirst | ImagingResponseFlagLast),
      0, sizeof(s_pixels), 4, 2, ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels,
      sizeof(s_pixels));
  prv_receive(buf, len);
  cl_assert_equal_i(s_notif_deliveries, 1);

  len = prv_build_response(buf, 7, ImagingResponseFlagLast, 2, 2, 0, 0, 0, nullptr, 0, s_pixels + 2,
                           2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_failures, 0);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(memcmp(prv_image_pixels(s_last_image), s_pixels, sizeof(s_pixels)) == 0);
}

#ifdef CONFIG_IMAGE_CACHE
static const ImageCacheKey s_key = {.bytes = {1, 2, 3}};

void test_imaging__received_into_cache(void) {
  cl_assert(imaging_request_album_art(TEST_TOKEN, ImagingFormat4BitPalette, 4, 2, "Title", "Artist",
                                      &s_key));
  uint8_t buf[64];
  size_t len =
      prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst, 0, 2, 4, 2,
                         ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  // The transfer holds the PSRAM, and takes no internal RAM
  cl_assert_equal_i(fake_psram_holds(), 1);
  cl_assert_equal_i(psram_image_pool_used(), 1);

  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagLast, 2, 2, 0, 0, 0, nullptr, 0,
                           s_pixels + 2, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(fake_psram_holds(), 0);
  cl_assert_equal_i(s_deliveries, 1);
  // The consumer gets a copy in internal RAM
  cl_assert(memcmp(prv_image_pixels(s_last_image), s_pixels, sizeof(s_pixels)) == 0);
  cl_assert_equal_i(resident_image_get_palette(s_last_image)[2].argb, s_palette[2]);
  cl_assert(image_cache_contains(ImageCacheClientAlbumArt, &s_key, GSize(4, 2)));
}

void test_imaging__dropped_transfer_aborts_cache_entry(void) {
  cl_assert(imaging_request_album_art(TEST_TOKEN, ImagingFormat4BitPalette, 4, 2, "Title", "Artist",
                                      &s_key));
  uint8_t buf[64];
  const size_t len =
      prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst, 0, 2, 4, 2,
                         ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  const PebbleCommSessionEvent closed_event = {
    .is_open = false,
    .is_system = true,
  };
  imaging_handle_comm_session_event(&closed_event);
  cl_assert_equal_i(s_failures, 1);
  cl_assert_equal_i(fake_psram_holds(), 0);
  cl_assert_equal_i(psram_image_pool_used(), 0);
  cl_assert(!image_cache_contains(ImageCacheClientAlbumArt, &s_key, GSize(4, 2)));
}

void test_imaging__uncached_without_key(void) {
  cl_assert(imaging_request_album_art(TEST_TOKEN, ImagingFormat4BitPalette, 4, 2, "Title", "Artist",
                                      nullptr));
  prv_receive_valid_image(TEST_TOKEN);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert_equal_i(psram_image_pool_used(), 0);
}

void test_imaging__notification_cached_by_item(void) {
  const Uuid id = UuidMake(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  cl_assert(imaging_request_notification_image(9, ImagingFormat4BitPalette, 4, 2, &id));
  uint8_t buf[64];
  const size_t len = prv_build_response(
      buf, 9,
      prv_typed(ImagingImageTypeNotification, ImagingResponseFlagFirst | ImagingResponseFlagLast),
      0, sizeof(s_pixels), 4, 2, ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels,
      sizeof(s_pixels));
  prv_receive(buf, len);
  cl_assert_equal_i(s_notif_deliveries, 1);
  ImageCacheKey key;
  image_cache_key_from_uuid(&key, &id);
  cl_assert(image_cache_contains(ImageCacheClientNotification, &key, GSize(4, 2)));
}
#else
// The tests generator ignores the preprocessor: only test_imaging_cache runs these
void test_imaging__received_into_cache(void) {
}

void test_imaging__dropped_transfer_aborts_cache_entry(void) {
}

void test_imaging__uncached_without_key(void) {
}

void test_imaging__notification_cached_by_item(void) {
}
#endif
