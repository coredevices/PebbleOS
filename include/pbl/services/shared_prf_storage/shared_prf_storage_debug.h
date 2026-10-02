/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_shared_prf_storage_shared_prf_storage_debug Shared PRF storage debug
 * @ingroup services_shared_prf_storage
 * @brief Shell dump of the shared PRF storage.
 * @{
 */

struct pbl_shell;

/**
 * @brief Print the shared PRF storage contents.
 *
 * Only built with the shell in non-release builds.
 *
 * @param sh Shell to print to.
 */
void shared_prf_storage_dump_contents(const struct pbl_shell *sh);

/** @} */
