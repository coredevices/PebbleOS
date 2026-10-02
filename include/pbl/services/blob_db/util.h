/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "api.h"

/**
 * @defgroup services_blob_db_util Utilities
 * @ingroup services_blob_db
 * @brief BlobDB helpers.
 * @{
 */

/**
 * @brief Free a dirty list.
 *
 * @param dirty_list List head, must not be NULL.
 */
void blob_db_util_free_dirty_list(BlobDBDirtyItem *dirty_list);

/** @} */
