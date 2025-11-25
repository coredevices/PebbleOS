/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! Auto-shutdown when idle in PRF to increase the changes of getting Pebbles shipped
//! that have some level of battery charge in them.

//! Start listening for battery connection, bluetooth connection, and button events to feed a
//! watchdog.
void prf_idle_watchdog_start(void);

//! Stop the watchdog. We will no longer reset if events don't occur frequently enough.
void prf_idle_watchdog_stop(void);
