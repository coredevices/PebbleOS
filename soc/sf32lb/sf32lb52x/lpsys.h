/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Keep LPSYS awake so that its registers can be accessed. Reference counted, shared with the
// LCPU boot code.
void soc_sf32lb52_lpsys_wake(void);

void soc_sf32lb52_lpsys_release(void);
