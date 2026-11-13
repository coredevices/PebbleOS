/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/malloc.h>
#include <pbl/services/notifications/notification_image.h>

#include <applib/graphics/gtypes.h>
#include <clar.h>

static const Uuid s_ids[] = {
  UuidMake(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
  UuidMake(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2),
  UuidMake(3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3),
  UuidMake(4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4),
};

static int s_destroyed;

void imaging_image_destroy(GBitmap *bitmap) {
  if (bitmap) {
    s_destroyed++;
  }
  pbl_free(bitmap);
}

static const GBitmap *prv_show(const Uuid *id) {
  GBitmap *bitmap = pbl_zalloc(sizeof(*bitmap));
  uint8_t token;

  cl_assert(notification_image_claim(id, &token));
  cl_assert(notification_image_is_pending(id));
  cl_assert(notification_image_store(token, bitmap));
  return bitmap;
}

static void prv_assert_image(const Uuid *id, const GBitmap *bitmap) {
  cl_assert(notification_image_lock(id) == bitmap);
  notification_image_unlock();
}

void test_notification_image__initialize(void) {
  s_destroyed = 0;
}

void test_notification_image__cleanup(void) {
  notification_image_clear();
}

void test_notification_image__image_of_the_claimed_item(void) {
  const GBitmap *first = prv_show(&s_ids[0]);
  uint8_t token;

  prv_assert_image(&s_ids[0], first);
  prv_assert_image(&s_ids[1], nullptr);
  cl_assert(!notification_image_claim(&s_ids[0], &token));
}

void test_notification_image__recent_image_needs_no_request(void) {
  const GBitmap *first = prv_show(&s_ids[0]);
  const GBitmap *second = prv_show(&s_ids[1]);
  uint8_t token;

  prv_assert_image(&s_ids[0], nullptr);

  cl_assert(!notification_image_claim(&s_ids[0], &token));
  cl_assert(!notification_image_is_pending(&s_ids[0]));
  prv_assert_image(&s_ids[0], first);

  cl_assert(!notification_image_claim(&s_ids[1], &token));
  prv_assert_image(&s_ids[1], second);
  cl_assert_equal_i(s_destroyed, 0);
}

void test_notification_image__oldest_recent_image_is_dropped(void) {
  prv_show(&s_ids[0]);
  const GBitmap *second = prv_show(&s_ids[1]);
  prv_show(&s_ids[2]);
  prv_show(&s_ids[3]);
  uint8_t token;

  cl_assert_equal_i(s_destroyed, 1);
  cl_assert(!notification_image_claim(&s_ids[1], &token));
  prv_assert_image(&s_ids[1], second);

  cl_assert(notification_image_claim(&s_ids[0], &token));
  cl_assert(notification_image_is_pending(&s_ids[0]));
}

void test_notification_image__response_after_a_recent_image_is_dropped(void) {
  GBitmap *late = pbl_zalloc(sizeof(*late));
  const GBitmap *first = prv_show(&s_ids[0]);
  uint8_t token;
  uint8_t unused;

  cl_assert(notification_image_claim(&s_ids[1], &token));
  cl_assert(!notification_image_claim(&s_ids[0], &unused));

  cl_assert(!notification_image_store(token, late));
  cl_assert_equal_i(s_destroyed, 1);
  prv_assert_image(&s_ids[0], first);
}

void test_notification_image__clear_frees_every_image(void) {
  prv_show(&s_ids[0]);
  prv_show(&s_ids[1]);
  prv_show(&s_ids[2]);

  notification_image_clear();

  cl_assert_equal_i(s_destroyed, 3);
  prv_assert_image(&s_ids[2], nullptr);
}
