/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <pbl_arch_barrier.h>

/**
 * @defgroup kernel_barrier Memory barriers
 * @ingroup kernel
 * @brief Ordering of memory and device accesses as other bus masters see them.
 *
 * Needed where memory is shared with a DMA engine or another core: after filling a buffer and
 * before the register write that starts the device, and after reading the register that says
 * it is done and before reading what it wrote. Each barrier also stops the compiler from
 * moving memory accesses across it.
 * @{
 */

/** @brief Order earlier memory and device accesses before later ones. */
static PBL_ALWAYS_INLINE void pbl_mb(void) {
  arch_mb();
}

/** @brief Order earlier memory and device reads before later ones. */
static PBL_ALWAYS_INLINE void pbl_rmb(void) {
  arch_rmb();
}

/** @brief Order earlier memory and device writes before later ones. */
static PBL_ALWAYS_INLINE void pbl_wmb(void) {
  arch_wmb();
}

/** @} */
