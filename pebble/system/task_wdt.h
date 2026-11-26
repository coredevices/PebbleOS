/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef CONFIG_WATCHDOG_CALLBACK
//! Record where the CPU was as a reboot reason when the hardware watchdog expires, where it
//! warns before resetting.
void task_wdt_watch_hw_expiry(void);
#endif
