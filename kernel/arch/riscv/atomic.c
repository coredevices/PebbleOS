/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <pbl_arch_irq.h>

// The atomic operations the compiler calls out for, without the A extension: with every
// interrupt masked, on the one core there is

#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"

#define ATOMIC_FETCH_OP(name, size, type, expr)                                           \
  PBL_USED type __atomic_fetch_##name##_##size(volatile void *ptr, type val, int order) { \
    volatile type *p = ptr;                                                               \
    unsigned int key = arch_irq_disable_all();                                            \
    type old = *p;                                                                        \
                                                                                          \
    *p = (expr);                                                                          \
    arch_irq_restore_all(key);                                                            \
    (void)order;                                                                          \
                                                                                          \
    return old;                                                                           \
  }                                                                                       \
                                                                                          \
  PBL_USED type __atomic_##name##_fetch_##size(volatile void *ptr, type val, int order) { \
    volatile type *p = ptr;                                                               \
    unsigned int key = arch_irq_disable_all();                                            \
    type old = *p;                                                                        \
    type new = (expr);                                                                    \
                                                                                          \
    *p = new;                                                                             \
    arch_irq_restore_all(key);                                                            \
    (void)order;                                                                          \
                                                                                          \
    return new;                                                                           \
  }

#define ATOMIC_OPS(size, type)                                                                     \
  ATOMIC_FETCH_OP(add, size, type, old + val)                                                      \
  ATOMIC_FETCH_OP(sub, size, type, old - val)                                                      \
  ATOMIC_FETCH_OP(and, size, type, old &val)                                                       \
  ATOMIC_FETCH_OP(or, size, type, old | val)                                                       \
  ATOMIC_FETCH_OP(xor, size, type, old ^ val)                                                      \
  ATOMIC_FETCH_OP(nand, size, type, ~(old & val))                                                  \
                                                                                                   \
  PBL_USED type __atomic_exchange_##size(volatile void *ptr, type val, int order) {                \
    volatile type *p = ptr;                                                                        \
    unsigned int key = arch_irq_disable_all();                                                     \
    type old = *p;                                                                                 \
                                                                                                   \
    *p = val;                                                                                      \
    arch_irq_restore_all(key);                                                                     \
    (void)order;                                                                                   \
                                                                                                   \
    return old;                                                                                    \
  }                                                                                                \
                                                                                                   \
  PBL_USED bool __atomic_compare_exchange_##size(volatile void *ptr, void *expected, type desired, \
                                                 bool weak, int success, int failure) {            \
    volatile type *p = ptr;                                                                        \
    type *e = expected;                                                                            \
    unsigned int key = arch_irq_disable_all();                                                     \
    type old = *p;                                                                                 \
    bool equal = old == *e;                                                                        \
                                                                                                   \
    if (equal) {                                                                                   \
      *p = desired;                                                                                \
    } else {                                                                                       \
      *e = old;                                                                                    \
    }                                                                                              \
    arch_irq_restore_all(key);                                                                     \
    (void)weak;                                                                                    \
    (void)success;                                                                                 \
    (void)failure;                                                                                 \
                                                                                                   \
    return equal;                                                                                  \
  }

ATOMIC_OPS(1, uint8_t)
ATOMIC_OPS(2, uint16_t)
ATOMIC_OPS(4, uint32_t)
