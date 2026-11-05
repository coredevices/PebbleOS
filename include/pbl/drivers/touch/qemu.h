/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/touch.h>

/**
 * @defgroup drivers_touch_qemu QEMU
 * @ingroup drivers_touch
 * @brief QEMU touchscreen, covering the display.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_touch pbl_touch_qemu;
/** @endcond */

/** @brief The QEMU touchscreen. */
#define QEMU_TOUCH (&pbl_touch_qemu)

/** @} */
