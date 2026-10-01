/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/imaging.h"

#include "applib/graphics/gtypes.h"
#include "pbl/services/comm_session/session.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Stubs & Fakes
///////////////////////////////////////////////////////////

#include "fake_session.h"
#include "fake_system_task.h"

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

// Delivery capture
///////////////////////////////////////////////////////////

static int s_deliveries;
static int s_will_receives;
static int s_failures;
static uint8_t s_last_token;
static uint8_t s_last_failure_token;
static GBitmap *s_last_bitmap;
static ImagingAlbumArt *s_last_art;

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

static void prv_free_last_bitmap(void) {
  if (s_last_bitmap) {
    kernel_free(s_last_bitmap->addr);
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

static void prv_segmented_art_handler(uint8_t token, ImagingAlbumArt *art) {
  s_deliveries++;
  s_last_token = token;
  imaging_album_art_free(s_last_art);
  s_last_art = art;
}

static void prv_will_receive_handler(uint8_t token) {
  s_will_receives++;
}

static void prv_failure_handler(uint8_t token) {
  s_failures++;
  s_last_failure_token = token;
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
  imaging_protocol_msg_callback(NULL, msg, length);
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

static void prv_receive_encoded_art(const uint8_t *encoded, uint16_t encoded_len,
                                    uint32_t declared_len) {
  uint8_t buf[128];
  const size_t head = sizeof(ImagingResponseHeader) + 6 + sizeof(s_palette);
  const size_t length = prv_build_response(
      buf, TEST_TOKEN, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, encoded_len, 4, 2,
      ImagingFormat4BitPaletteLz4, s_palette, sizeof(s_palette), encoded, encoded_len);
  memmove(buf + head + 4, buf + head, encoded_len);
  for (unsigned int i = 0; i < 4; ++i)
    buf[head + i] = declared_len >> (i * 8);
  prv_receive(buf, length + 4);
}

// Tests
///////////////////////////////////////////////////////////

static Transport *s_transport;

void test_imaging__initialize(void) {
  fake_comm_session_init();
  s_transport = fake_transport_create(TransportDestinationSystem, NULL, NULL);
  fake_transport_set_connected(s_transport, true);
  imaging_register_handler(ImagingImageTypeAlbumArt, prv_art_handler);
  imaging_register_album_art_handler(NULL);
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
  s_last_bitmap = NULL;
  s_last_art = NULL;
  stub_pbl_malloc_set_kernel_malloc_should_fail(false);
}

void test_imaging__cleanup(void) {
  prv_free_last_bitmap();
  imaging_album_art_free(s_last_art);
  s_last_art = NULL;
  fake_comm_session_cleanup();
}

void test_imaging__single_chunk_image(void) {
  prv_receive_valid_image(TEST_TOKEN);
  cl_assert_equal_i(s_will_receives, 1);
  cl_assert_equal_i(s_failures, 0);
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

void test_imaging__compressed_album_art_literal_tile(void) {
  imaging_register_album_art_handler(prv_segmented_art_handler);
  // One 4x2 tile: LZ4 token with four literal bytes and no match.
  const uint8_t encoded[] = {5, 0, 0x40, 0x01, 0x20, 0x12, 0x01};
  prv_receive_encoded_art(encoded, sizeof(encoded), sizeof(encoded));
  cl_assert_equal_i(s_failures, 0);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(s_last_art != NULL);
  cl_assert_equal_i(imaging_album_art_width(s_last_art), 4);
  cl_assert_equal_i(imaging_album_art_height(s_last_art), 2);
  cl_assert_equal_i(imaging_album_art_row_size(s_last_art), 2);
  cl_assert_equal_i(imaging_album_art_palette(s_last_art)[1], s_palette[1]);
  uint8_t decoded[4];
  cl_assert(imaging_album_art_decode_tile(s_last_art, 0, decoded, sizeof(decoded)));
  cl_assert_equal_i(memcmp(decoded, s_pixels, sizeof(decoded)), 0);
  cl_assert(!imaging_album_art_decode_tile(s_last_art, 1, decoded, sizeof(decoded)));
  cl_assert(!imaging_album_art_decode_tile(s_last_art, 0, decoded, 3));
}

void test_imaging__compressed_album_art_rejects_bad_tiles(void) {
  imaging_register_album_art_handler(prv_segmented_art_handler);
  // LZ4 back reference at the start has no preceding bytes to refer to.
  const uint8_t bad_offset[] = {3, 0, 0, 1, 0};
  prv_receive_encoded_art(bad_offset, sizeof(bad_offset), sizeof(bad_offset));
  cl_assert_equal_i(s_failures, 1);
  cl_assert_equal_i(s_deliveries, 0);
  // Raw tile must have exactly row_size*height bytes.
  const uint8_t short_raw[] = {3, 0x80, 1, 2, 3};
  prv_receive_encoded_art(short_raw, sizeof(short_raw), sizeof(short_raw));
  cl_assert_equal_i(s_failures, 2);
  // Declared stream length and chunk bytes must agree.
  const uint8_t good_raw[] = {4, 0x80, 1, 2, 3, 4};
  prv_receive_encoded_art(good_raw, sizeof(good_raw), sizeof(good_raw) + 1);
  cl_assert_equal_i(s_failures, 3);
  const uint8_t zero_offset[] = {3, 0, 0, 0, 0};
  prv_receive_encoded_art(zero_offset, sizeof(zero_offset), sizeof(zero_offset));
  const uint8_t truncated_extension[] = {2, 0, 0xf0, 0xff};
  prv_receive_encoded_art(truncated_extension, sizeof(truncated_extension),
                          sizeof(truncated_extension));
  const uint8_t literal_overrun[] = {2, 0, 0x40, 0};
  prv_receive_encoded_art(literal_overrun, sizeof(literal_overrun), sizeof(literal_overrun));
  const uint8_t match_overrun[] = {4, 0, 0x10, 0x11, 1, 0};
  prv_receive_encoded_art(match_overrun, sizeof(match_overrun), sizeof(match_overrun));
  cl_assert_equal_i(s_failures, 7);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__raw_full_screen_album_art_uses_segments(void) {
  imaging_register_album_art_handler(prv_segmented_art_handler);
  stub_pbl_malloc_reset_kernel_malloc_max_requested();
  uint8_t buf[600];
  uint8_t pixels[512];
  memset(pixels, 0x12, sizeof(pixels));
  const uint32_t total = 130 * 260;
  for (uint32_t offset = 0; offset < total; offset += sizeof(pixels)) {
    const uint16_t chunk = MIN(sizeof(pixels), total - offset);
    const uint8_t flags = (offset == 0 ? ImagingResponseFlagFirst : 0) |
                          (offset + chunk == total ? ImagingResponseFlagLast : 0);
    const size_t length =
        prv_build_response(buf, TEST_TOKEN, flags, offset, chunk, 260, 260,
                           ImagingFormat4BitPalette, s_palette, sizeof(s_palette), pixels, chunk);
    prv_receive(buf, length);
  }
  cl_assert_equal_i(s_failures, 0);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(s_last_art != NULL);
  cl_assert(stub_pbl_malloc_get_kernel_malloc_max_requested() <= 1500);
  uint8_t decoded[1300];
  cl_assert(imaging_album_art_decode_tile(s_last_art, 25, decoded, sizeof(decoded)));
  for (size_t i = 0; i < sizeof(decoded); ++i)
    cl_assert_equal_i(decoded[i], 0x12);
}

static void prv_check_phone_vector(const char *name) {
  char path[512];
  snprintf(path, sizeof(path), "%simaging/%s.packets", CLAR_FIXTURE_PATH, name);
  FILE *packets = fopen(path, "rb");
  cl_assert(packets != NULL);
  snprintf(path, sizeof(path), "%simaging/%s.raw", CLAR_FIXTURE_PATH, name);
  FILE *expected = fopen(path, "rb");
  cl_assert(expected != NULL);
  imaging_register_album_art_handler(prv_segmented_art_handler);
  stub_pbl_malloc_reset_kernel_malloc_max_requested();
  uint8_t message[1100] = {ImagingCmdIDResponse};
  for (;;) {
    uint8_t count[2];
    const size_t read_count = fread(count, 1, 2, packets);
    if (!read_count)
      break;
    cl_assert_equal_i(read_count, 2);
    const size_t length = count[0] | (count[1] << 8);
    cl_assert(length < sizeof(message) - 1);
    cl_assert_equal_i(fread(message + 1, 1, length, packets), length);
    prv_receive(message, length + 1);
  }
  fclose(packets);
  cl_assert_equal_i(s_failures, 0);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(s_last_art != NULL);
  cl_assert(stub_pbl_malloc_get_kernel_malloc_max_requested() <= 1500);
  const uint16_t width = imaging_album_art_width(s_last_art);
  const uint16_t height = imaging_album_art_height(s_last_art);
  const size_t row_size = imaging_album_art_row_size(s_last_art);
  cl_assert_equal_i(width, 260);
  cl_assert_equal_i(height, 260);
  cl_assert_equal_i(row_size, 130);
  uint8_t decoded[1300];
  uint8_t reference[1300];
  for (uint16_t tile = 0; tile < 26; ++tile) {
    cl_assert(imaging_album_art_decode_tile(s_last_art, tile, decoded, sizeof(decoded)));
    cl_assert_equal_i(fread(reference, 1, sizeof(reference), expected), sizeof(reference));
    cl_assert_equal_i(memcmp(decoded, reference, sizeof(reference)), 0);
  }
  cl_assert_equal_i(fgetc(expected), EOF);
  fclose(expected);
}

void test_imaging__kotlin_gabbro_vector_matches_all_pixels(void) {
  prv_check_phone_vector("gabbro");
}

void test_imaging__kotlin_incompressible_vector_matches_all_pixels(void) {
  prv_check_phone_vector("random");
}

void test_imaging__multi_chunk_image(void) {
  uint8_t buf[64];
  size_t len =
      prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst, 0, 2, 4, 2,
                         ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagLast, 2, 2, 0, 0, 0, NULL, 0,
                           s_pixels + 2, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(s_last_bitmap != NULL);
  cl_assert(memcmp(s_last_bitmap->addr, s_pixels, sizeof(s_pixels)) == 0);
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
                                        NULL, 0, NULL, 0);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 1);
  cl_assert(s_last_bitmap == NULL);
}

void test_imaging__truncated_header_rejected(void) {
  uint8_t buf[64];
  const size_t len = prv_build_response(
      buf, TEST_TOKEN, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, sizeof(s_pixels), 4,
      2, ImagingFormat4BitPalette, s_palette, sizeof(s_palette), s_pixels, sizeof(s_pixels));
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
  uint8_t big_palette[17] = {0};
  uint8_t buf[64];
  // More palette entries than a 4-bpp image can have
  size_t len = prv_build_response(
      buf, TEST_TOKEN, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, 4, 4, 2,
      ImagingFormat4BitPalette, big_palette, sizeof(big_palette), s_pixels, 4);
  prv_receive(buf, len);
  // A palettized format with no palette at all
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0,
                           4, 4, 2, ImagingFormat4BitPalette, NULL, 0, s_pixels, 4);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__oversized_image_rejected(void) {
  uint8_t buf[64];
  // 300x300 8-bit = 90000 bytes, over IMAGING_MAX_BYTES
  const size_t len =
      prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagFirst | ImagingResponseFlagLast, 0, 4,
                         300, 300, ImagingFormat8BitColor, NULL, 0, s_pixels, 4);
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
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagLast, 3, 1, 0, 0, 0, NULL, 0,
                           s_pixels + 3, 1);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
  cl_assert_equal_i(s_failures, 1);
  cl_assert_equal_i(s_last_failure_token, TEST_TOKEN);
  // The transfer was reset: a well-formed follow-up chunk must also be ignored
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagLast, 2, 2, 0, 0, 0, NULL, 0,
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
  len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagLast, 2, 60, 0, 0, 0, NULL, 0,
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
  len = prv_build_response(buf, TEST_TOKEN + 1, ImagingResponseFlagLast, 2, 2, 0, 0, 0, NULL, 0,
                           s_pixels + 2, 2);
  prv_receive(buf, len);
  cl_assert_equal_i(s_deliveries, 0);
}

void test_imaging__unsupported_latches_until_session_close(void) {
  cl_assert(imaging_is_type_supported(ImagingImageTypeAlbumArt));

  uint8_t buf[32];
  const size_t len = prv_build_response(buf, TEST_TOKEN, ImagingResponseFlagUnsupported, 0, 0, 0, 0,
                                        0, NULL, 0, NULL, 0);
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
      buf, TEST_TOKEN, prv_typed(ImagingImageTypeNotification, ImagingResponseFlagUnsupported), 0,
      0, 0, 0, 0, NULL, 0, NULL, 0);
  prv_receive(buf, len);
  cl_assert_equal_i(s_notif_deliveries, 1);
  cl_assert(!imaging_is_type_supported(ImagingImageTypeNotification));
  cl_assert(imaging_is_type_supported(ImagingImageTypeAlbumArt));
}
