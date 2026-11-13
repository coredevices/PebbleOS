/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/mutex.h>
#include <pbl/services/imaging.h>
#include <pbl/services/notifications/notification_image.h>
#include <pbl/util/size.h>

#include <applib/graphics/gtypes.h>

//! Responses are delivered on KernelMain while the card that requested and renders the image runs
//! on KernelMain (modal) or the App task (notification history), so the slot needs a lock. It is
//! recursive because the renderer holds it across the blit to stop the bitmap being freed
//! underneath it.
static PBL_MUTEX_DEFINE(s_lock);

// Most recent first: the image of the item the slot belongs to, then those of the items before it
static struct {
  Uuid item_id;
  GBitmap *bitmap;
} s_images[1 + CONFIG_NOTIFICATION_IMAGE_RECENTS];
static uint8_t s_token;
static bool s_claimed; //!< The slot belongs to s_images[0].item_id (in flight, delivered or empty).
static bool s_pending; //!< Waiting on a response for s_token.

void notification_image_service_init(void) {
}

bool notification_image_claim(const Uuid *item_id, uint8_t *token_out) {
  const size_t last = ARRAY_LENGTH(s_images) - 1;

  if (!item_id || !token_out) {
    return false;
  }
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_claimed && uuid_equal(&s_images[0].item_id, item_id)) {
    pbl_mutex_unlock(&s_lock);
    return false;
  }

  for (size_t i = 1; i <= last; i++) {
    if (s_images[i].bitmap && uuid_equal(&s_images[i].item_id, item_id)) {
      typeof(s_images[0]) image = s_images[i];

      // Shown before: it comes back to the front, and whatever was in flight is dropped
      memmove(&s_images[1], &s_images[0], i * sizeof(s_images[0]));
      s_images[0] = image;
      s_claimed = true;
      s_pending = false;
      pbl_mutex_unlock(&s_lock);
      return false;
    }
  }

  if (s_images[0].bitmap) {
    imaging_image_destroy(s_images[last].bitmap);
    memmove(&s_images[1], &s_images[0], last * sizeof(s_images[0]));
  }
  s_images[0].bitmap = nullptr;
  s_images[0].item_id = *item_id;
  s_claimed = true;
  s_pending = true;
  *token_out = ++s_token;
  pbl_mutex_unlock(&s_lock);
  return true;
}

const GBitmap *notification_image_lock(const Uuid *item_id) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  // Held until notification_image_unlock so the bitmap can't be freed mid-draw.
  if (!item_id || !s_claimed || !uuid_equal(&s_images[0].item_id, item_id)) {
    return nullptr;
  }
  return s_images[0].bitmap;
}

void notification_image_unlock(void) {
  pbl_mutex_unlock(&s_lock);
}

bool notification_image_is_pending(const Uuid *item_id) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool pending =
      s_pending && s_claimed && item_id && uuid_equal(&s_images[0].item_id, item_id);
  pbl_mutex_unlock(&s_lock);
  return pending;
}

bool notification_image_store(uint8_t token, GBitmap *bitmap) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_pending || token != s_token) {
    pbl_mutex_unlock(&s_lock);
    imaging_image_destroy(bitmap);
    return false;
  }
  s_pending = false;
  s_images[0].bitmap = bitmap;
  pbl_mutex_unlock(&s_lock);
  return true;
}

void notification_image_clear(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  for (size_t i = 0; i < ARRAY_LENGTH(s_images); i++) {
    imaging_image_destroy(s_images[i].bitmap);
    s_images[i].bitmap = nullptr;
  }
  s_claimed = false;
  s_pending = false;
  pbl_mutex_unlock(&s_lock);
}
