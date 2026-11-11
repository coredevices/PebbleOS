/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/als.h>

/**
 * @defgroup drivers_als_qemu QEMU
 * @ingroup drivers_als
 * @brief QEMU ambient light sensor. QEMU emulates none, so it always reads complete darkness.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_als pbl_als_qemu;
/** @endcond */

/** @brief The QEMU ambient light sensor. */
#define QEMU_ALS (&pbl_als_qemu)

/** @} */
