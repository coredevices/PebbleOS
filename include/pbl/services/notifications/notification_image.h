/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/util/uuid.h>

struct GBitmap;

/**
 * @defgroup services_notifications_notification_image Notification images
 * @ingroup services_notifications
 * @brief Single slot holding the phone-supplied image of the notification on screen.
 *
 * Only one notification card is focused at a time, so one slot is enough. A card claims the slot
 * for its item, requests the image from the phone with the returned token, and draws whatever has
 * been stored while holding the slot lock. Responses are delivered on KernelMain while cards render
 * on KernelMain or the App task, so all functions are thread safe.
 * @{
 */

/**
 * @brief Whether the platform supports notification images.
 *
 * Images need a color display and enough RAM for the decoded bitmap, so only emery and gabbro
 * qualify, the same bar as album art.
 */
#if defined(CONFIG_PLATFORM_EMERY) || defined(CONFIG_PLATFORM_GABBRO)
#define NOTIFICATION_IMAGE_SUPPORTED 1
#else
#define NOTIFICATION_IMAGE_SUPPORTED 0
#endif

/** @brief Initialize the service. Call once at boot before any other function here. */
void notification_image_service_init(void);

/**
 * @brief Claim the slot for an item.
 *
 * Drops any image held for a previous item.
 *
 * @param item_id Item to claim the slot for.
 * @param[out] token_out Token to send with the image request.
 * @return false if the slot already belongs to @p item_id, whether the image is in flight,
 *         delivered or the phone said it has none, so callers can drive this from a redraw; also
 *         false if an argument is NULL.
 */
bool notification_image_claim(const Uuid *item_id, uint8_t *token_out);

/**
 * @brief Lock the slot and get the image of an item.
 *
 * The slot stays locked until notification_image_unlock(), which must be called even when this
 * returns NULL. The lock is recursive.
 *
 * @param item_id Item whose image to get.
 * @return Stored bitmap, owned by the slot, or NULL if there is none for @p item_id.
 */
const struct GBitmap *notification_image_lock(const Uuid *item_id);

/** @brief Unlock the slot locked by notification_image_lock(). */
void notification_image_unlock(void);

/**
 * @brief Check whether the image of an item is still being fetched.
 *
 * Lets a card show a placeholder instead of an empty band until the phone answers.
 *
 * @param item_id Item to check.
 * @return true while the response for @p item_id is outstanding.
 */
bool notification_image_is_pending(const Uuid *item_id);

/**
 * @brief Store a delivered image.
 *
 * Takes ownership of @p bitmap; it, its pixel data and its palette must be allocated on the kernel
 * heap. A stale token frees the bitmap.
 *
 * @param token Token returned by notification_image_claim().
 * @param bitmap Delivered image, or NULL when the phone had no image.
 * @return true if the slot resolved, so anything showing the notification must redraw, also when
 *         @p bitmap is NULL as that removes the pending placeholder.
 */
bool notification_image_store(uint8_t token, struct GBitmap *bitmap);

/** @brief Release the slot and free the stored image. */
void notification_image_clear(void);

/** @} */
