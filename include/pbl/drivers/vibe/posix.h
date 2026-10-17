/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/vibe.h>

/**
 * @defgroup drivers_vibe_posix POSIX
 * @ingroup drivers_vibe
 * @brief Vibration motor of a native build, which does nothing.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_vibe pbl_vibe_posix;
/** @endcond */

/** @brief The native vibration motor. */
#define POSIX_VIBE (&pbl_vibe_posix)

/** @} */
