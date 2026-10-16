/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/backlight.h>

/**
 * @defgroup drivers_backlight_posix POSIX
 * @ingroup drivers_backlight
 * @brief Backlight of a native build, which does nothing.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_backlight pbl_backlight_posix;
/** @endcond */

/** @brief The native backlight. */
#define POSIX_BACKLIGHT (&pbl_backlight_posix)

/** @} */
