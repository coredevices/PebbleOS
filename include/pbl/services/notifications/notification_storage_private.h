/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Notification storage file size
#define NOTIFICATION_STORAGE_FILE_SIZE (30 * 1024)

//! Minimum increment of space to free up when compressing.
//! The higher the value, the less often we need to compress,
//! but we will lose more notifications
#define NOTIFICATION_STORAGE_MINIMUM_INCREMENT_SIZE (NOTIFICATION_STORAGE_FILE_SIZE / 4)

