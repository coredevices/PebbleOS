/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/runlevel.h>

/**
 * @defgroup services_services_normal Normal firmware services
 * @ingroup services_services
 * @brief Services only present in the normal (non-recovery) firmware.
 * @{
 */

/** @brief Early initialization the kernel depends on: initializes the filesystem. */
void services_normal_early_init(void);

/** @brief Initialize the normal firmware services. */
void services_normal_init(void);

/**
 * @brief Apply a runlevel to the normal firmware services.
 *
 * @param runlevel Runlevel to apply.
 */
void services_normal_set_runlevel(RunLevel runlevel);

/** @} */
