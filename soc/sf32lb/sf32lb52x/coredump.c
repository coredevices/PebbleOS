/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "lpsys.h"

#include <pbl/coredump/coredump.h>
#include <pbl/util/units.h>

// Waking the LCPU busy-waits for its ack. If it is fully powered down this blocks until the
// watchdog reboots us, costing only the end of the dump. We reset right after, so the wake is
// never balanced.
PBL_COREDUMP_LATE_REGION_DEFINE(lcpu_ram, (const void *)0x20400000, PBL_KIB(64),
                                soc_sf32lb52_lpsys_wake);
