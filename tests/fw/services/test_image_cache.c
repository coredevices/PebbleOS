/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/services/image_cache.h>

#include <clar.h>
#include <fake_mutex.h>
#include <fake_new_timer.h>
#include <fake_psram.h>
#include <kernel/psram_image_pool.h>
#include <stubs_logging.h>

extern uint8_t s_slots[PSRAM_IMAGE_POOL_SLOTS][PSRAM_IMAGE_SLOT_SIZE];

uint16_t gbitmap_format_get_row_size_bytes(int16_t width, GBitmapFormat format) {
  return (format == GBitmapFormat4BitPalette) ? (width + 1) / 2 : width;
}

#define SIZE GSize(260, 260)

static ImageCacheKey prv_key(uint8_t n) {
  ImageCacheKey key = {};
  key.bytes[0] = n;
  return key;
}

static bool prv_put(ImageCacheClient client, uint8_t n) {
  const ImageCacheKey key = prv_key(n);
  ImageCacheEntry *entry = image_cache_begin(client, &key, SIZE, GBitmapFormat4BitPalette, 16);
  if (!entry) {
    return false;
  }
  memset(image_cache_pixels(entry), n, 130 * 260);
  image_cache_palette(entry)[0] = (GColor){.argb = n};
  image_cache_release(image_cache_commit(entry));
  return true;
}

static bool prv_has(ImageCacheClient client, uint8_t n) {
  const ImageCacheKey key = prv_key(n);
  return image_cache_contains(client, &key, SIZE);
}

static void prv_flush_all(void) {
  for (int c = 0; c < ImageCacheClientCount; c++) {
    image_cache_flush(c);
  }
}

void test_image_cache__initialize(void) {
  fake_psram_reset();
  psram_retain_acquire();
}

void test_image_cache__cleanup(void) {
  prv_flush_all();
  cl_assert_equal_i(psram_image_pool_used(), 0);
  psram_retain_release();
  cl_assert_equal_i(fake_psram_holds(), 0);
  fake_mutex_assert_all_unlocked();
}

void test_image_cache__hit_needs_key_and_size(void) {
  cl_assert(prv_put(ImageCacheClientAlbumArt, 1));
  cl_assert(prv_has(ImageCacheClientAlbumArt, 1));
  cl_assert(!prv_has(ImageCacheClientAlbumArt, 2));
  cl_assert(!prv_has(ImageCacheClientNotification, 1));
  const ImageCacheKey key = prv_key(1);
  cl_assert(!image_cache_contains(ImageCacheClientAlbumArt, &key, GSize(166, 166)));

  const GBitmap *bitmap = image_cache_acquire(ImageCacheClientAlbumArt, &key, SIZE);
  cl_assert(bitmap);
  cl_assert_equal_i(((uint8_t *)bitmap->addr)[1000], 1);
  cl_assert_equal_i(bitmap->palette[0].argb, 1);
  cl_assert_equal_i(bitmap->row_size_bytes, 130);
  image_cache_release(bitmap);
}

void test_image_cache__abort_frees_slot(void) {
  const ImageCacheKey key = prv_key(1);
  ImageCacheEntry *entry =
      image_cache_begin(ImageCacheClientAlbumArt, &key, SIZE, GBitmapFormat4BitPalette, 16);
  cl_assert(entry);
  cl_assert(!prv_has(ImageCacheClientAlbumArt, 1));
  image_cache_abort(entry);
  cl_assert_equal_i(psram_image_pool_used(), 0);
}

void test_image_cache__too_large_rejected(void) {
  const ImageCacheKey key = prv_key(1);
  cl_assert(
      !image_cache_begin(ImageCacheClientAlbumArt, &key, GSize(300, 300), GBitmapFormat8Bit, 0));
}

void test_image_cache__replacing_a_key(void) {
  cl_assert(prv_put(ImageCacheClientAlbumArt, 1));
  cl_assert(prv_put(ImageCacheClientAlbumArt, 1));
  cl_assert_equal_i(psram_image_pool_used(), 1);
}

void test_image_cache__client_max_evicts_own_lru(void) {
  // Album art may hold at most 12
  for (uint8_t n = 0; n < 12; n++) {
    cl_assert(prv_put(ImageCacheClientAlbumArt, n));
  }
  // Use 0, so 1 is the least recently used
  const ImageCacheKey key = prv_key(0);
  image_cache_release(image_cache_acquire(ImageCacheClientAlbumArt, &key, SIZE));

  cl_assert(prv_put(ImageCacheClientAlbumArt, 12));
  cl_assert_equal_i(psram_image_pool_used(), 12);
  cl_assert(prv_has(ImageCacheClientAlbumArt, 0));
  cl_assert(!prv_has(ImageCacheClientAlbumArt, 1));
  cl_assert(prv_has(ImageCacheClientAlbumArt, 12));
}

void test_image_cache__full_pool_respects_minimums(void) {
  // 18 slots: notifications take 16, album art 2
  for (uint8_t n = 0; n < 16; n++) {
    cl_assert(prv_put(ImageCacheClientNotification, n));
  }
  for (uint8_t n = 0; n < 2; n++) {
    cl_assert(prv_put(ImageCacheClientAlbumArt, n));
  }
  cl_assert_equal_i(psram_image_pool_used(), PSRAM_IMAGE_POOL_SLOTS);

  // Below its minimum, album art takes from notifications
  cl_assert(prv_put(ImageCacheClientAlbumArt, 2));
  cl_assert(prv_put(ImageCacheClientAlbumArt, 3));
  cl_assert(!prv_has(ImageCacheClientNotification, 0));
  cl_assert(!prv_has(ImageCacheClientNotification, 1));
  cl_assert(prv_has(ImageCacheClientNotification, 2));

  // At its minimum, it evicts its own
  cl_assert(prv_put(ImageCacheClientAlbumArt, 4));
  cl_assert(!prv_has(ImageCacheClientAlbumArt, 0));
  cl_assert(prv_has(ImageCacheClientNotification, 2));
}

void test_image_cache__pinned_entries_are_never_evicted(void) {
  const GBitmap *pinned[12];
  for (uint8_t n = 0; n < 12; n++) {
    cl_assert(prv_put(ImageCacheClientAlbumArt, n));
    const ImageCacheKey key = prv_key(n);
    pinned[n] = image_cache_acquire(ImageCacheClientAlbumArt, &key, SIZE);
  }
  cl_assert(!prv_put(ImageCacheClientAlbumArt, 12));
  for (uint8_t n = 0; n < 12; n++) {
    cl_assert(prv_has(ImageCacheClientAlbumArt, n));
    image_cache_release(pinned[n]);
  }
}

void test_image_cache__dropped_while_pinned(void) {
  cl_assert(prv_put(ImageCacheClientNotification, 1));
  const ImageCacheKey key = prv_key(1);
  const GBitmap *bitmap = image_cache_acquire(ImageCacheClientNotification, &key, SIZE);
  image_cache_retain(bitmap);

  image_cache_invalidate(ImageCacheClientNotification, &key);
  cl_assert(!prv_has(ImageCacheClientNotification, 1));
  // Still readable while pinned
  cl_assert_equal_i(((uint8_t *)bitmap->addr)[0], 1);
  cl_assert_equal_i(psram_image_pool_used(), 1);

  image_cache_release(bitmap);
  cl_assert_equal_i(psram_image_pool_used(), 1);
  image_cache_release(bitmap);
  cl_assert_equal_i(psram_image_pool_used(), 0);
}

void test_image_cache__lost_with_the_psram(void) {
  cl_assert(prv_put(ImageCacheClientAlbumArt, 1));
  psram_retain_release();
  fake_psram_power_down();
  memset(s_slots, 0xa5, sizeof(s_slots));
  cl_assert(!prv_has(ImageCacheClientAlbumArt, 1));

  psram_retain_acquire();
  const ImageCacheKey key = prv_key(1);
  cl_assert(!image_cache_acquire(ImageCacheClientAlbumArt, &key, SIZE));
  // The lost entry is reclaimed for the next one
  cl_assert(prv_put(ImageCacheClientAlbumArt, 2));
  cl_assert_equal_i(psram_image_pool_used(), 1);
}

void test_image_cache__activity_holds_until_idle(void) {
  psram_retain_release();
  cl_assert_equal_i(fake_psram_holds(), 0);

  image_cache_activity();
  cl_assert_equal_i(fake_psram_holds(), 1);
  image_cache_activity();
  cl_assert_equal_i(fake_psram_holds(), 1);
  cl_assert_equal_i(stub_new_timer_timeout(stub_new_timer_get_next()),
                    CONFIG_PSRAM_IDLE_RELEASE_MS);

  cl_assert(stub_new_timer_fire(stub_new_timer_get_next()));
  cl_assert_equal_i(fake_psram_holds(), 0);

  psram_retain_acquire();
}

void test_image_cache__dropped_while_written(void) {
  const ImageCacheKey key = prv_key(1);
  ImageCacheEntry *entry =
      image_cache_begin(ImageCacheClientNotification, &key, SIZE, GBitmapFormat4BitPalette, 16);
  memset(image_cache_pixels(entry), 1, 130 * 260);
  // The item was edited while its image was in flight
  image_cache_invalidate(ImageCacheClientNotification, &key);
  cl_assert_equal_i(psram_image_pool_used(), 1);

  const GBitmap *bitmap = image_cache_commit(entry);
  cl_assert_equal_i(((uint8_t *)bitmap->addr)[0], 1);
  cl_assert(!prv_has(ImageCacheClientNotification, 1));
  image_cache_release(bitmap);
  cl_assert_equal_i(psram_image_pool_used(), 0);
}
