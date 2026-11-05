/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <pbl/util/uuid.h>

#include <applib/graphics/gtypes.h>

/**
 * @defgroup services_image_cache Image cache
 * @ingroup services
 * @brief Decoded images in disposable external RAM, keyed per client.
 *
 * Each entry takes one slot of the PSRAM image pool. Entries only live as long as the PSRAM is
 * retained: every entry records the retention epoch it was written in, and one from another epoch
 * is a miss. The cache never takes a retention hold itself, so callers must hold one while they
 * write or read entries; @ref image_cache_activity keeps one for a while after user activity.
 *
 * Least recently used entries are evicted first, starting with the requesting client's. Each
 * client is guaranteed a minimum number of entries and limited to a maximum. Acquired entries are
 * pinned and never evicted.
 *
 * @code{.c}
 * psram_retain_acquire();
 * const GBitmap *bitmap = image_cache_acquire(ImageCacheClientAlbumArt, &key, size);
 * if (bitmap) {
 *   ResidentImage *copy = resident_image_create_from_bitmap(bitmap);
 *   image_cache_release(bitmap);
 * }
 * psram_retain_release();
 * @endcode
 * @{
 */

/** @brief Cache clients, each with its own quota. */
typedef enum {
  /** Album art of music tracks. */
  ImageCacheClientAlbumArt,
  /** Images attached to notifications. */
  ImageCacheClientNotification,

  /** Number of clients. */
  ImageCacheClientCount,
} ImageCacheClient;

/** @brief Key of an entry, unique within a client. */
typedef struct {
  /** Key bytes. */
  uint8_t bytes[16];
} ImageCacheKey;

/** @brief Entry being written. */
typedef struct ImageCacheEntry ImageCacheEntry;

/**
 * @brief Make the key of an item from its UUID.
 *
 * @param[out] key Key.
 * @param uuid UUID.
 */
static inline void image_cache_key_from_uuid(ImageCacheKey *key, const Uuid *uuid) {
  static_assert(sizeof(key->bytes) == sizeof(*uuid), "a UUID is a key");
  memcpy(key->bytes, uuid, sizeof(key->bytes));
}

#ifdef CONFIG_IMAGE_CACHE

/**
 * @brief Start writing an entry.
 *
 * Replaces any entry with the same key. A retention hold must be taken until the entry is
 * committed or aborted.
 *
 * @param client Client.
 * @param key Key.
 * @param size Size in pixels.
 * @param format Pixel format.
 * @param palette_count Number of palette entries, at most 16.
 * @return Entry, or NULL if the image does not fit a slot or every slot is pinned.
 */
ImageCacheEntry *image_cache_begin(ImageCacheClient client, const ImageCacheKey *key, GSize size,
                                   GBitmapFormat format, uint8_t palette_count);

/**
 * @brief Get the pixels of an entry being written, in rows of the format's row size.
 *
 * @param entry Entry.
 * @return Pixels.
 */
uint8_t *image_cache_pixels(ImageCacheEntry *entry);

/**
 * @brief Get the palette of an entry being written.
 *
 * @param entry Entry.
 * @return Palette, with room for 16 entries.
 */
GColor *image_cache_palette(ImageCacheEntry *entry);

/**
 * @brief Finish writing an entry, making it visible and pinning it.
 *
 * An entry dropped while it was written is not made visible, but its bitmap is still valid until
 * released.
 *
 * @param entry Entry.
 * @return Bitmap of the entry, valid until image_cache_release().
 */
const GBitmap *image_cache_commit(ImageCacheEntry *entry);

/**
 * @brief Abandon an entry being written, freeing its slot.
 *
 * @param entry Entry.
 */
void image_cache_abort(ImageCacheEntry *entry);

/**
 * @brief Look up and pin an entry.
 *
 * A retention hold must be taken until the entry is released.
 *
 * @param client Client.
 * @param key Key.
 * @param size Size the image must have.
 * @return Bitmap, valid until image_cache_release(), or NULL on a miss.
 */
const GBitmap *image_cache_acquire(ImageCacheClient client, const ImageCacheKey *key, GSize size);

/**
 * @brief Pin an entry again.
 *
 * @param bitmap Bitmap returned by image_cache_acquire(), not released yet.
 */
void image_cache_retain(const GBitmap *bitmap);

/**
 * @brief Unpin an entry.
 *
 * @param bitmap Bitmap returned by image_cache_acquire().
 */
void image_cache_release(const GBitmap *bitmap);

/**
 * @brief Check whether an entry is cached, without a retention hold.
 *
 * @param client Client.
 * @param key Key.
 * @param size Size the image must have.
 * @return true if image_cache_acquire() would hit, as long as the PSRAM is not lost meanwhile.
 */
bool image_cache_contains(ImageCacheClient client, const ImageCacheKey *key, GSize size);

/**
 * @brief Drop an entry. A pinned entry, or one being written, is freed once released.
 *
 * @param client Client.
 * @param key Key.
 */
void image_cache_invalidate(ImageCacheClient client, const ImageCacheKey *key);

/**
 * @brief Drop every entry of a client.
 *
 * @param client Client.
 */
void image_cache_flush(ImageCacheClient client);

/**
 * @brief Report user activity.
 *
 * Keeps the PSRAM retained, and so the cache, until @c CONFIG_PSRAM_IDLE_RELEASE_MS after the
 * last activity. Only for activity a user drives, such as a button press: periodic redraws must
 * not call it.
 */
void image_cache_activity(void);

#else

static inline ImageCacheEntry *image_cache_begin(ImageCacheClient client, const ImageCacheKey *key,
                                                 GSize size, GBitmapFormat format,
                                                 uint8_t palette_count) {
  return NULL;
}

static inline uint8_t *image_cache_pixels(ImageCacheEntry *entry) {
  return NULL;
}

static inline GColor *image_cache_palette(ImageCacheEntry *entry) {
  return NULL;
}

static inline const GBitmap *image_cache_commit(ImageCacheEntry *entry) {
  return NULL;
}

static inline void image_cache_abort(ImageCacheEntry *entry) {
}

static inline const GBitmap *image_cache_acquire(ImageCacheClient client, const ImageCacheKey *key,
                                                 GSize size) {
  return NULL;
}

static inline void image_cache_retain(const GBitmap *bitmap) {
}

static inline void image_cache_release(const GBitmap *bitmap) {
}

static inline bool image_cache_contains(ImageCacheClient client, const ImageCacheKey *key,
                                        GSize size) {
  return false;
}

static inline void image_cache_invalidate(ImageCacheClient client, const ImageCacheKey *key) {
}

static inline void image_cache_flush(ImageCacheClient client) {
}

static inline void image_cache_activity(void) {
}

#endif

/** @} */
