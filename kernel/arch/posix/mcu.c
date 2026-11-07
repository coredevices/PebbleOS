/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/irq.h>
#include <pbl/mcu/cache.h>
#include <pbl/mcu/fpu.h>

#include <arch.h>

// The host has no interrupt controller, caches to maintain or MPU: interrupts
// come from host threads (see native.c) and memory is unprotected.

void pbl_irq_init(void) {
}

void pbl_irq_enable(pbl_irq_t irq) {
}

void pbl_irq_disable(pbl_irq_t irq) {
}

bool pbl_irq_is_enabled(pbl_irq_t irq) {
  return true;
}

void pbl_irq_set_pending(pbl_irq_t irq) {
}

void pbl_irq_clear_pending(pbl_irq_t irq) {
}

void mcu_fpu_cleanup(void) {
}

bool icache_is_enabled(void) {
  return false;
}

void icache_invalidate(void *addr, size_t size) {
}

void icache_align(uintptr_t *addr, size_t *size) {
}

bool dcache_is_enabled(void) {
  return false;
}

void dcache_flush(const void *addr, size_t size) {
}

void dcache_invalidate(void *addr, size_t size) {
}

void dcache_align(uintptr_t *addr, size_t *size) {
}

void arch_mpu_init(void) {
}

void arch_mpu_enable(void) {
}

void arch_mpu_region_set(unsigned int slot, const struct pbl_mpu_region *region) {
}

void arch_mpu_user_ro_write(int slot, void *dst, const void *src, size_t n) {
  memcpy(dst, src, n);
}

bool arch_mpu_region_get(unsigned int slot, struct pbl_mpu_region *region) {
  return false;
}
