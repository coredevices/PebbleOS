/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_services Service initialization
 * @ingroup services
 * @brief Boot-time initialization of all services.
 *
 * Covers the common services and, except in the recovery firmware, the normal firmware ones.
 * @{
 */

/** @brief Initialize the services the kernel depends on. */
void services_early_init(void);

/** @brief Initialize all services. */
void services_init(void);

/** @} */
