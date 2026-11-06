/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

// Keep LPSYS awake so that its registers can be accessed. Reference counted, shared with the
// LCPU boot code.
void soc_sf32lb52_lpsys_wake(void);

void soc_sf32lb52_lpsys_release(void);

// Reset the LCPU and hold it, with the radio and, if asked, all the other LPSYS blocks reset too.
// LPSYS must be awake. No effect if the LCPU is held already.
void soc_sf32lb52_lpsys_halt(bool all);
