/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/temp.h>

/**
 * @defgroup drivers_temp_qemu QEMU
 * @ingroup drivers_temp
 * @brief QEMU temperature sensor. QEMU emulates none, so it reports a fixed 25 degrees Celsius.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_temp pbl_temp_qemu;
/** @endcond */

/** @brief The QEMU temperature sensor. */
#define QEMU_TEMP (&pbl_temp_qemu)

/** @} */
