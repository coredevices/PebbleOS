/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup mcu_fpu FPU
 * @ingroup mcu
 * @brief Floating-point unit helpers.
 * @{
 */

/**
 * @brief Drop the calling thread's floating-point context.
 *
 * Clears CONTROL.FPCA, so context switches stop stacking the FPU registers (an extra 132 bytes
 * of stack) until the thread uses the FPU again. Call where no floating-point values are live,
 * e.g. between events of an event loop.
 */
void mcu_fpu_cleanup(void);

/** @} */
