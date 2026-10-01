/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Implemented by the SoC. Runs on the ISR stack once .data, .bss and
//! .ramfunc are set up, before main().
void pbl_soc_early_init(void);
