/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/vibe.h>

/**
 * @defgroup drivers_vibe_qemu QEMU
 * @ingroup drivers_vibe
 * @brief QEMU vibration motor, reporting its state to the QEMU host.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_vibe pbl_vibe_qemu;
/** @endcond */

/** @brief The QEMU vibration motor. */
#define QEMU_VIBE (&pbl_vibe_qemu)

/** @} */
