/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

/**
 * @defgroup services_blob_db_faithful Faithfulness
 * @ingroup services_blob_db
 * @brief Whether the databases are those of the phone the watch is bonded with.
 *
 * The watch stops being faithful when it bonds with a new phone, and is again once that phone
 * has flushed the databases. The phone is told in the version message, and syncs from scratch
 * with a watch that is not.
 * @{
 */

/**
 * @brief Check whether the watch is faithful.
 *
 * @return True if faithful; false by default.
 */
bool blob_db_is_faithful(void);

/**
 * @brief Set whether the watch is faithful.
 *
 * @param faithful New state.
 */
void blob_db_set_faithful(bool faithful);

/** @} */
