/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/als.h>

/**
 * @defgroup drivers_als_posix POSIX
 * @ingroup drivers_als
 * @brief Ambient light sensor of a native build: always dark.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_als pbl_als_posix;
/** @endcond */

/** @brief The POSIX ambient light sensor. */
#define POSIX_ALS (&pbl_als_posix)

/** @} */
