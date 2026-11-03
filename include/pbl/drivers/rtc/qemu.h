/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/rtc.h>

/**
 * @defgroup drivers_rtc_qemu QEMU
 * @ingroup drivers_rtc
 * @brief QEMU RTC, following the host clock.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_rtc pbl_rtc_qemu;
/** @endcond */

/** @brief The QEMU RTC. */
#define QEMU_RTC (&pbl_rtc_qemu)

/** @} */
