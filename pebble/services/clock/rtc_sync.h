/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Write the wall clock to the RTC.
void rtc_sync_save(void);

// Write the wall clock to the RTC periodically from now on.
void rtc_sync_start(void);
