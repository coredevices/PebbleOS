/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Zephyr's atomics with the interrupt lock instead of the compiler builtins.
//
// The builtins are LDREX/STREX loops. The host's RAM is the worker RAM, which
// the kernel tasks' MPU region (and so the host threads') marks inner
// shareable; an exclusive store to shareable memory needs a global exclusive
// monitor, and without one it never succeeds, so the loops never end. With a
// single core, the interrupt lock is atomic enough.

#include <stdbool.h>
#include <stddef.h>

#include <pbl/kernel/irq.h>

#include <zephyr/sys/atomic_types.h>

static inline bool atomic_cas(atomic_t *target, atomic_val_t old_value, atomic_val_t new_value) {
  pbl_irq_lock();
  const bool swapped = *target == old_value;
  if (swapped) {
    *target = new_value;
  }
  pbl_irq_unlock();
  return swapped;
}

static inline bool atomic_ptr_cas(atomic_ptr_t *target, atomic_ptr_val_t old_value,
                                  atomic_ptr_val_t new_value) {
  pbl_irq_lock();
  const bool swapped = *target == old_value;
  if (swapped) {
    *target = new_value;
  }
  pbl_irq_unlock();
  return swapped;
}

// Sets *target to @p expr (which can read the previous value as `old`) and
// returns the previous value.
#define ZBT_ATOMIC_OP(target, expr)     \
  ({                                    \
    pbl_irq_lock();                     \
    const atomic_val_t old = *(target); \
    *(target) = (expr);                 \
    pbl_irq_unlock();                   \
    old;                                \
  })

static inline atomic_val_t atomic_add(atomic_t *target, atomic_val_t value) {
  return ZBT_ATOMIC_OP(target, old + value);
}

static inline atomic_val_t atomic_sub(atomic_t *target, atomic_val_t value) {
  return ZBT_ATOMIC_OP(target, old - value);
}

static inline atomic_val_t atomic_inc(atomic_t *target) {
  return atomic_add(target, 1);
}

static inline atomic_val_t atomic_dec(atomic_t *target) {
  return atomic_sub(target, 1);
}

static inline atomic_val_t atomic_get(const atomic_t *target) {
  return *(const volatile atomic_t *)target;
}

static inline atomic_ptr_val_t atomic_ptr_get(const atomic_ptr_t *target) {
  return *(const volatile atomic_ptr_t *)target;
}

static inline atomic_val_t atomic_set(atomic_t *target, atomic_val_t value) {
  return ZBT_ATOMIC_OP(target, value);
}

static inline atomic_ptr_val_t atomic_ptr_set(atomic_ptr_t *target, atomic_ptr_val_t value) {
  pbl_irq_lock();
  const atomic_ptr_val_t old = *target;
  *target = value;
  pbl_irq_unlock();
  return old;
}

static inline atomic_val_t atomic_clear(atomic_t *target) {
  return atomic_set(target, 0);
}

static inline atomic_ptr_val_t atomic_ptr_clear(atomic_ptr_t *target) {
  return atomic_ptr_set(target, NULL);
}

static inline atomic_val_t atomic_or(atomic_t *target, atomic_val_t value) {
  return ZBT_ATOMIC_OP(target, old | value);
}

static inline atomic_val_t atomic_xor(atomic_t *target, atomic_val_t value) {
  return ZBT_ATOMIC_OP(target, old ^ value);
}

static inline atomic_val_t atomic_and(atomic_t *target, atomic_val_t value) {
  return ZBT_ATOMIC_OP(target, old & value);
}

static inline atomic_val_t atomic_nand(atomic_t *target, atomic_val_t value) {
  return ZBT_ATOMIC_OP(target, ~(old & value));
}
