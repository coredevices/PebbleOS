/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/flash.h>

/**
 * @defgroup drivers_flash_qemu QEMU
 * @ingroup drivers_flash
 * @brief @ref drivers_flash driver for the QEMU external flash, memory-mapped at its base.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_flash pbl_flash_qemu;
/** @endcond */

/** @brief The QEMU external flash. */
#define QEMU_FLASH (&pbl_flash_qemu)

/** @} */
