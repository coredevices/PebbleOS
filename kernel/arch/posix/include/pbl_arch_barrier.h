/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdatomic.h>

#define arch_mb()  atomic_thread_fence(memory_order_seq_cst)
#define arch_rmb() atomic_thread_fence(memory_order_acquire)
#define arch_wmb() atomic_thread_fence(memory_order_release)
