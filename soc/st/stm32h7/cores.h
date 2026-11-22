/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Hardware semaphores of the power-off of a dual-core part: the Cortex-M7 releases the first,
// which the Cortex-M4 watches, and the Cortex-M4 takes the second once it is done
#define STM32H7_CM4_SEM_POWER_OFF 0U
#define STM32H7_CM4_SEM_ACK       1U
