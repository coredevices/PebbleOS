/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <pbl_arch_mmio.h>

/**
 * @defgroup kernel_mmio MMIO
 * @ingroup kernel
 * @brief Memory-mapped I/O register access.
 *
 * Each access is a single load or store of the given width. Accesses to the same device stay
 * in program order, but are not ordered against normal memory: a driver that hands memory to a
 * DMA engine orders them with pbl_wmb() and pbl_rmb():
 *
 * @code{.c}
 * buf[0] = cmd;
 * pbl_wmb();
 * pbl_writel(DMA_START, cfg->base + DMA_CTRL);
 *
 * while ((pbl_readl(cfg->base + DMA_STATUS) & STATUS_DONE) == 0U) {
 * }
 * pbl_rmb();
 * reply = buf[1];
 * @endcode
 * @{
 */

/**
 * @brief Read an 8-bit register.
 *
 * @param addr Register address.
 * @return Register value.
 */
static PBL_ALWAYS_INLINE uint8_t pbl_readb(uintptr_t addr) {
  return arch_readb(addr);
}

/**
 * @brief Read a 16-bit register.
 *
 * @param addr Register address, 2-byte aligned.
 * @return Register value.
 */
static PBL_ALWAYS_INLINE uint16_t pbl_readw(uintptr_t addr) {
  return arch_readw(addr);
}

/**
 * @brief Read a 32-bit register.
 *
 * @param addr Register address, 4-byte aligned.
 * @return Register value.
 */
static PBL_ALWAYS_INLINE uint32_t pbl_readl(uintptr_t addr) {
  return arch_readl(addr);
}

/**
 * @brief Write an 8-bit register.
 *
 * @param val Value to write.
 * @param addr Register address.
 */
static PBL_ALWAYS_INLINE void pbl_writeb(uint8_t val, uintptr_t addr) {
  arch_writeb(val, addr);
}

/**
 * @brief Write a 16-bit register.
 *
 * @param val Value to write.
 * @param addr Register address, 2-byte aligned.
 */
static PBL_ALWAYS_INLINE void pbl_writew(uint16_t val, uintptr_t addr) {
  arch_writew(val, addr);
}

/**
 * @brief Write a 32-bit register.
 *
 * @param val Value to write.
 * @param addr Register address, 4-byte aligned.
 */
static PBL_ALWAYS_INLINE void pbl_writel(uint32_t val, uintptr_t addr) {
  arch_writel(val, addr);
}

/** @} */
