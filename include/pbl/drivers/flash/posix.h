/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/flash.h>

/**
 * @defgroup drivers_flash_posix POSIX
 * @ingroup drivers_flash
 * @brief @ref drivers_flash driver for a native build: the flash is a file mapped into memory.
 *
 * Writes and erases land in the file right away, so its contents survive a restart.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_flash pbl_flash_posix;
/** @endcond */

/** @brief The native external flash. */
#define POSIX_FLASH (&pbl_flash_posix)

/** @} */
