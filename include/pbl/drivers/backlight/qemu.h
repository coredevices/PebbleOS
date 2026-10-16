/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/backlight.h>

/**
 * @defgroup drivers_backlight_qemu QEMU
 * @ingroup drivers_backlight
 * @brief QEMU backlight, driving the pebble-display brightness (or RGB, with
 * @c CONFIG_BACKLIGHT_QEMU_COLOR) registers.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_backlight pbl_backlight_qemu;
/** @endcond */

/** @brief The QEMU backlight. */
#define QEMU_BACKLIGHT (&pbl_backlight_qemu)

/** @} */
