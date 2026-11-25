/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Ask the Cortex-M7 to bring the clock tree back, this core woken alone from Stop
void pbl_soc_stm32h7_cm4_clocks_request(void);
