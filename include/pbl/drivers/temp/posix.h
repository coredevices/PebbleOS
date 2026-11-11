/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/temp.h>

/**
 * @defgroup drivers_temp_posix POSIX
 * @ingroup drivers_temp
 * @brief Temperature sensor of a native build, which reports a fixed 25 degrees Celsius.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_temp pbl_temp_posix;
/** @endcond */

/** @brief The native temperature sensor. */
#define POSIX_TEMP (&pbl_temp_posix)

/** @} */
