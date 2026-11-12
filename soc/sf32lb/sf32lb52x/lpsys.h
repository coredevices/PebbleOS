/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Keep LPSYS awake so that its registers can be accessed. Reference counted.
void soc_sf32lb_lpsys_wake(void);

void soc_sf32lb_lpsys_release(void);

// Reset the LCPU and hold it, with the radio and, if asked, all the other LPSYS blocks reset too.
// LPSYS must be awake. No effect if the LCPU is held already.
void soc_sf32lb_lpsys_halt(bool all);

// Set the stack pointer and entry point the LCPU boots with. LPSYS must be awake.
void soc_sf32lb_lpsys_set_entry(uint32_t sp, uint32_t pc);

// Let a halted LCPU run. LPSYS must be awake.
void soc_sf32lb_lpsys_run(void);
