/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup drivers_flash_flash_internal Flash internals
 * @ingroup drivers_flash
 * @brief Internal hooks of the flash API.
 * @{
 */

/** @brief Initialize flash_erase_optimal_range() support; called by flash_init(). */
void flash_erase_init(void);

/** @} */
