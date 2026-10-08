/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/runlevel.h>

/**
 * @defgroup services_services_common Common services
 * @ingroup services_services
 * @brief Services present in both the normal and the recovery firmware.
 * @{
 */

/** @brief Initialize the common services. */
void services_common_init(void);

/**
 * @brief Apply a runlevel to the common services.
 *
 * @param runlevel Runlevel to apply.
 */
void services_common_set_runlevel(RunLevel runlevel);

/** @} */
