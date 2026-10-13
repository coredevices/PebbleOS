/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Cortex-M cores are in order and do not reorder memory against device
// accesses; only the compiler can.
#define arch_mb()  __asm volatile("" ::: "memory")
#define arch_rmb() __asm volatile("" ::: "memory")
#define arch_wmb() __asm volatile("" ::: "memory")
