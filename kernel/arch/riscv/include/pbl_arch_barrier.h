/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define arch_mb()  __asm volatile("fence rw, rw" ::: "memory")
#define arch_rmb() __asm volatile("fence r, r" ::: "memory")
#define arch_wmb() __asm volatile("fence w, w" ::: "memory")
