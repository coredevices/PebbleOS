/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/rtc.h>

/**
 * @defgroup drivers_rtc_sf32lb SF32LB
 * @ingroup drivers_rtc
 * @brief SiFli SF32LB calendar RTC, counting clk_rtc at its calibrated rate.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_rtc pbl_rtc_sf32lb;
/** @endcond */

/** @brief The SF32LB RTC. */
#define SF32LB_RTC (&pbl_rtc_sf32lb)

/** @} */
