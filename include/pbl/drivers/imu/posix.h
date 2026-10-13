/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/accel.h>

/**
 * @defgroup drivers_accel_posix POSIX accelerometer
 * @ingroup drivers_accel
 * @brief Accelerometer of a native build: a watch lying still, face up.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_accel pbl_accel_posix;
/** @endcond */

/** @brief The POSIX accelerometer. */
#define POSIX_ACCEL (&pbl_accel_posix)

/** @} */
