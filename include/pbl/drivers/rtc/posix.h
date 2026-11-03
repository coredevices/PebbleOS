/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/rtc.h>

/**
 * @defgroup drivers_rtc_posix POSIX
 * @ingroup drivers_rtc
 * @brief RTC following the host clock.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_rtc pbl_rtc_posix;
/** @endcond */

/** @brief The host RTC. */
#define POSIX_RTC (&pbl_rtc_posix)

/** @} */
