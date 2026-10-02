/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_notifications_notification_storage_private Notification storage sizing
 * @ingroup services_notifications
 * @brief Size of the notification storage file.
 * @{
 */

/** @brief Size of the notification storage file, in bytes. */
#define NOTIFICATION_STORAGE_FILE_SIZE (30 * 1024)

/**
 * @brief Minimum amount of space, in bytes, freed when storage is full.
 *
 * Larger values compress less often but drop more notifications.
 */
#define NOTIFICATION_STORAGE_MINIMUM_INCREMENT_SIZE (NOTIFICATION_STORAGE_FILE_SIZE / 4)

/** @} */
