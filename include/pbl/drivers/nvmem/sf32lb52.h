/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/nvmem.h>

/**
 * @defgroup drivers_nvmem_sf32lb52 SF32LB52 eFuse
 * @ingroup drivers_nvmem
 * @brief @ref drivers_nvmem provider of the SF32LB52 eFuse controller. Read-only.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_nvmem pbl_nvmem_sf32lb52_efuse;
/** @endcond */

/** eFuse, four 32-byte banks. */
#define SF32LB52_EFUSE (&pbl_nvmem_sf32lb52_efuse)

/** @} */
