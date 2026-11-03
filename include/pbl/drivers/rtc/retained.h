/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/rtc.h>

/**
 * @defgroup drivers_rtc_retained Retained RAM
 * @ingroup drivers_rtc
 * @brief RTC for SoCs without a calendar: the time last set, kept in retained RAM.
 *
 * It survives resets, not power loss, and does not count while the system is off: the time
 * resumes from the value last set, so it needs setting often.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_rtc pbl_rtc_retained;
/** @endcond */

/** @brief The retained RAM RTC. */
#define RETAINED_RTC (&pbl_rtc_retained)

/** @} */
