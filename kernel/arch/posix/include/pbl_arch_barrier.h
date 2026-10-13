/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define arch_mb()  __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define arch_rmb() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define arch_wmb() __atomic_thread_fence(__ATOMIC_RELEASE)
