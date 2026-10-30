/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/image_cache.h>

#ifdef CONFIG_IMAGE_CACHE

#include <inttypes.h>
#include <string.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/psram.h>
#include <pbl/logging/logging.h>
#include <pbl/services/analytics/analytics.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/util/size.h>

#include <kernel/psram_image_pool.h>

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>
#endif

PBL_LOG_MODULE_DECLARE(service_imaging, DEFAULT_LOG_LEVEL);

// Slot layout: the palette, then the pixels
#define PALETTE_ENTRIES (16)
#define PIXELS_OFFSET   (32)
static_assert(PALETTE_ENTRIES * sizeof(GColor) <= PIXELS_OFFSET, "the palette fits");

typedef enum {
  EntryStateFree,
  EntryStateWriting,
  EntryStateCommitted,
} EntryState;

// In SRAM, so lookups don't need the PSRAM
struct ImageCacheEntry {
  EntryState state;
  ImageCacheClient client;
  //! Dropped while pinned: freed once released, invisible meanwhile
  bool dropped;
  uint8_t pins;
  ImageCacheKey key;
  uint32_t epoch;
  uint32_t last_use;
  uint8_t *slot;
  GBitmap bitmap;
};

typedef struct {
  uint8_t min;
  uint8_t max;
} Quota;

static const Quota s_quotas[ImageCacheClientCount] = {
  [ImageCacheClientAlbumArt] = {.min = 4, .max = 12},
  [ImageCacheClientNotification] = {.min = 4, .max = 16},
};

static ImageCacheEntry s_entries[PSRAM_IMAGE_POOL_SLOTS];
static uint32_t s_clock;
static PBL_MUTEX_DEFINE(s_lock);

static PBL_MUTEX_DEFINE(s_activity_lock);
static TimerID s_activity_timer = TIMER_INVALID_ID;
static bool s_activity_held;

static void prv_free_locked(ImageCacheEntry *entry) {
  PBL_ASSERTN(entry->pins == 0U);
  psram_image_pool_free(entry->slot);
  *entry = (ImageCacheEntry){};
}

static bool prv_is_lost(const ImageCacheEntry *entry) {
  return (entry->state != EntryStateFree) && (entry->epoch != psram_epoch());
}

// Free the entries whose contents were lost with the PSRAM
static void prv_reclaim_lost_locked(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_entries); i++) {
    if (prv_is_lost(&s_entries[i])) {
      // Pinning takes a hold, so no pinned entry can be lost
      prv_free_locked(&s_entries[i]);
    }
  }
}

static bool prv_matches(const ImageCacheEntry *entry, ImageCacheClient client,
                        const ImageCacheKey *key) {
  return (entry->state != EntryStateFree) && !entry->dropped && (entry->client == client) &&
         (memcmp(&entry->key, key, sizeof(*key)) == 0);
}

static ImageCacheEntry *prv_find_locked(ImageCacheClient client, const ImageCacheKey *key,
                                        GSize size) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_entries); i++) {
    ImageCacheEntry *entry = &s_entries[i];
    if ((entry->state == EntryStateCommitted) && prv_matches(entry, client, key) &&
        !prv_is_lost(entry) && (entry->bitmap.bounds.size.w == size.w) &&
        (entry->bitmap.bounds.size.h == size.h)) {
      return entry;
    }
  }
  return NULL;
}

static void prv_drop_locked(ImageCacheEntry *entry) {
  // The writer still owns an entry being written, until it commits or aborts
  if ((entry->pins > 0U) || (entry->state == EntryStateWriting)) {
    entry->dropped = true;
  } else {
    prv_free_locked(entry);
  }
}

static unsigned int prv_count_locked(ImageCacheClient client) {
  unsigned int count = 0;
  for (size_t i = 0; i < ARRAY_LENGTH(s_entries); i++) {
    if ((s_entries[i].state != EntryStateFree) && (s_entries[i].client == client)) {
      count++;
    }
  }
  return count;
}

// Least recently used committed, unpinned entry, of the client or of the others over their minimum
static ImageCacheEntry *prv_victim_locked(ImageCacheClient client, bool own) {
  ImageCacheEntry *victim = NULL;
  for (size_t i = 0; i < ARRAY_LENGTH(s_entries); i++) {
    ImageCacheEntry *entry = &s_entries[i];
    if ((entry->state != EntryStateCommitted) || (entry->pins > 0U)) {
      continue;
    }
    if (own ? (entry->client != client)
            : ((entry->client == client) ||
               (prv_count_locked(entry->client) <= s_quotas[entry->client].min))) {
      continue;
    }
    if (!victim || (entry->last_use < victim->last_use)) {
      victim = entry;
    }
  }
  return victim;
}

static bool prv_evict_locked(ImageCacheClient client) {
  ImageCacheEntry *victim;

  if (prv_count_locked(client) >= s_quotas[client].max) {
    victim = prv_victim_locked(client, true);
  } else if (prv_count_locked(client) < s_quotas[client].min) {
    // Below its minimum, a client takes from the others first
    victim = prv_victim_locked(client, false);
    if (!victim) {
      victim = prv_victim_locked(client, true);
    }
  } else {
    victim = prv_victim_locked(client, true);
    if (!victim) {
      victim = prv_victim_locked(client, false);
    }
  }

  if (!victim) {
    return false;
  }

  PBL_LOG_DBG("Evict client=%d", (int)victim->client);
  PBL_ANALYTICS_ADD(image_cache_evictions, 1);
  prv_free_locked(victim);
  return true;
}

static ImageCacheEntry *prv_free_entry_locked(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_entries); i++) {
    if (s_entries[i].state == EntryStateFree) {
      return &s_entries[i];
    }
  }
  return NULL;
}

ImageCacheEntry *image_cache_begin(ImageCacheClient client, const ImageCacheKey *key, GSize size,
                                   GBitmapFormat format, uint8_t palette_count) {
  ImageCacheEntry *entry = NULL;
  uint8_t *slot = NULL;

  PBL_ASSERTN(client < ImageCacheClientCount);
  PBL_ASSERTN(psram_is_retained());

  const uint16_t row_size_bytes = gbitmap_format_get_row_size_bytes(size.w, format);
  if ((size.w <= 0) || (size.h <= 0) || (row_size_bytes == 0U) ||
      (palette_count > PALETTE_ENTRIES) ||
      ((uint32_t)row_size_bytes * size.h > PSRAM_IMAGE_SLOT_SIZE - PIXELS_OFFSET)) {
    return NULL;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  prv_reclaim_lost_locked();

  for (size_t i = 0; i < ARRAY_LENGTH(s_entries); i++) {
    if (prv_matches(&s_entries[i], client, key)) {
      prv_drop_locked(&s_entries[i]);
    }
  }

  if ((prv_count_locked(client) < s_quotas[client].max) || prv_evict_locked(client)) {
    slot = psram_image_pool_alloc();
    if (!slot && prv_evict_locked(client)) {
      slot = psram_image_pool_alloc();
    }
  }

  if (!slot) {
    PBL_LOG_WRN("No image slot for client=%d: all pinned", (int)client);
    goto unlock;
  }

  entry = prv_free_entry_locked();
  PBL_ASSERTN(entry);
  *entry = (ImageCacheEntry){
    .state = EntryStateWriting,
    .client = client,
    .key = *key,
    .epoch = psram_epoch(),
    .slot = slot,
    .bitmap = {
      .addr = slot + PIXELS_OFFSET,
      .row_size_bytes = row_size_bytes,
      .info.format = format,
      .info.version = GBITMAP_VERSION_CURRENT,
      .bounds = {GPointZero, size},
      .palette = (palette_count > 0U) ? (GColor *)slot : NULL,
    },
  };

unlock:
  pbl_mutex_unlock(&s_lock);
  return entry;
}

uint8_t *image_cache_pixels(ImageCacheEntry *entry) {
  return entry->bitmap.addr;
}

GColor *image_cache_palette(ImageCacheEntry *entry) {
  return (GColor *)entry->slot;
}

const GBitmap *image_cache_commit(ImageCacheEntry *entry) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  PBL_ASSERTN(entry->state == EntryStateWriting);
  entry->state = EntryStateCommitted;
  entry->last_use = ++s_clock;
  entry->pins++;
  pbl_mutex_unlock(&s_lock);
  return &entry->bitmap;
}

void image_cache_abort(ImageCacheEntry *entry) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  PBL_ASSERTN(entry->state == EntryStateWriting);
  prv_free_locked(entry);
  pbl_mutex_unlock(&s_lock);
}

const GBitmap *image_cache_acquire(ImageCacheClient client, const ImageCacheKey *key, GSize size) {
  const GBitmap *bitmap = NULL;

  PBL_ASSERTN(psram_is_retained());

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  ImageCacheEntry *entry = prv_find_locked(client, key, size);
  if (entry) {
    PBL_ASSERTN(entry->pins < UINT8_MAX);
    entry->pins++;
    entry->last_use = ++s_clock;
    bitmap = &entry->bitmap;
  }
  pbl_mutex_unlock(&s_lock);

  PBL_LOG_DBG("%s client=%d", bitmap ? "Hit" : "Miss", (int)client);
  if (bitmap) {
    PBL_ANALYTICS_ADD(image_cache_hits, 1);
  } else {
    PBL_ANALYTICS_ADD(image_cache_misses, 1);
  }

  return bitmap;
}

static ImageCacheEntry *prv_entry_of(const GBitmap *bitmap) {
  ImageCacheEntry *entry =
      (ImageCacheEntry *)((uintptr_t)bitmap - offsetof(ImageCacheEntry, bitmap));
  PBL_ASSERTN((entry >= s_entries) && (entry < &s_entries[ARRAY_LENGTH(s_entries)]) &&
              (entry->pins > 0U));
  return entry;
}

void image_cache_retain(const GBitmap *bitmap) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  ImageCacheEntry *entry = prv_entry_of(bitmap);
  PBL_ASSERTN(entry->pins < UINT8_MAX);
  entry->pins++;
  pbl_mutex_unlock(&s_lock);
}

void image_cache_release(const GBitmap *bitmap) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  ImageCacheEntry *entry = prv_entry_of(bitmap);
  entry->pins--;
  if ((entry->pins == 0U) && entry->dropped) {
    prv_free_locked(entry);
  }
  pbl_mutex_unlock(&s_lock);
}

bool image_cache_contains(ImageCacheClient client, const ImageCacheKey *key, GSize size) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool found = prv_find_locked(client, key, size) != NULL;
  pbl_mutex_unlock(&s_lock);
  return found;
}

void image_cache_invalidate(ImageCacheClient client, const ImageCacheKey *key) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  for (size_t i = 0; i < ARRAY_LENGTH(s_entries); i++) {
    if (prv_matches(&s_entries[i], client, key)) {
      prv_drop_locked(&s_entries[i]);
    }
  }
  pbl_mutex_unlock(&s_lock);
}

void image_cache_flush(ImageCacheClient client) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  for (size_t i = 0; i < ARRAY_LENGTH(s_entries); i++) {
    ImageCacheEntry *entry = &s_entries[i];
    if ((entry->state != EntryStateFree) && (entry->client == client) && !entry->dropped) {
      prv_drop_locked(entry);
    }
  }
  pbl_mutex_unlock(&s_lock);
}

static void prv_activity_timeout(void *data) {
  pbl_mutex_lock(&s_activity_lock, PBL_FOREVER);
  // Activity that raced the expiry restarted the timer: keep the hold
  if (s_activity_held && !new_timer_scheduled(s_activity_timer, NULL)) {
    s_activity_held = false;
    psram_retain_release();
  }
  pbl_mutex_unlock(&s_activity_lock);
}

void image_cache_activity(void) {
  pbl_mutex_lock(&s_activity_lock, PBL_FOREVER);
  if (s_activity_timer == TIMER_INVALID_ID) {
    s_activity_timer = new_timer_create();
  }
  if (!s_activity_held) {
    psram_retain_acquire();
    s_activity_held = true;
  }
  new_timer_start(s_activity_timer, CONFIG_PSRAM_IDLE_RELEASE_MS, prv_activity_timeout, NULL, 0);
  pbl_mutex_unlock(&s_activity_lock);
}

#ifdef CONFIG_SHELL
static int prv_cmd_cache(const struct pbl_shell *sh, size_t argc, char **argv) {
  static const char *const STATES[] = {"free", "writing", "committed"};

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  pbl_shell_print(sh, "Slots: %u of %u used, epoch %" PRIu32, psram_image_pool_used(),
                  (unsigned int)PSRAM_IMAGE_POOL_SLOTS, psram_epoch());
  for (size_t i = 0; i < ARRAY_LENGTH(s_entries); i++) {
    const ImageCacheEntry *entry = &s_entries[i];
    if (entry->state == EntryStateFree) {
      continue;
    }
    pbl_shell_print(sh, "%2u: client=%d %s%s%s %dx%d pins=%u use=%" PRIu32 " key=%02x%02x%02x%02x",
                    (unsigned int)i, (int)entry->client, STATES[entry->state],
                    entry->dropped ? " dropped" : "", prv_is_lost(entry) ? " lost" : "",
                    entry->bitmap.bounds.size.w, entry->bitmap.bounds.size.h, entry->pins,
                    entry->last_use, entry->key.bytes[0], entry->key.bytes[1], entry->key.bytes[2],
                    entry->key.bytes[3]);
  }
  pbl_mutex_unlock(&s_lock);
  return 0;
}

static const struct pbl_shell_cmd sub_image[] = {
  PBL_SHELL_CMD(cache, nullptr, "Dump the image cache entries", prv_cmd_cache),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(image, sub_image, "Image cache", nullptr);
#endif

#endif
