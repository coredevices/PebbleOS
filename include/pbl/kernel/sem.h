/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/types.h>

/**
 * @defgroup kernel_sem Semaphores
 * @ingroup kernel
 * @brief Counting semaphores; a binary semaphore has limit 1.
 *
 * A give hands the token directly to the highest priority waiter, so a woken taker never finds
 * the count stolen. Both operations may be called from ISRs; an ISR never blocks and gets
 * @c -EBUSY when no token is available.
 *
 * Signalling a thread from an interrupt:
 *
 * @code{.c}
 * static PBL_SEM_DEFINE(s_done, 0, 1);
 *
 * static void prv_dma_isr(void) {
 *   pbl_sem_give(&s_done);
 * }
 *
 * int transfer(void) {
 *   prv_dma_start();
 *   if (pbl_sem_take(&s_done, PBL_MSEC(100)) != 0) {
 *     prv_dma_abort();
 *     return -ETIMEDOUT;
 *   }
 *   return 0;
 * }
 * @endcode
 * @{
 */

/** @brief Counting semaphore. Usable from ISRs with @ref PBL_NO_WAIT. */
struct pbl_sem {
  /** Count restored by pbl_sem_reset(). */
  uint32_t initial;
  /** Maximum count; gives beyond it are dropped. */
  uint32_t limit;
  /** Backend state, including the current count. */
  struct pbl_sem_backend backend;
};

/**
 * @brief Static initializer for a semaphore.
 *
 * @param init Initial count.
 * @param lim Maximum count.
 */
#define PBL_SEM_INITIALIZER(init, lim) \
  {.initial = (init), .limit = (lim), .backend = PBL_SEM_BACKEND_INITIALIZER(init)}

/**
 * @brief Define a semaphore, usable without pbl_sem_init().
 *
 * @param name Name of the semaphore variable.
 * @param initial Initial count.
 * @param limit Maximum count.
 */
#define PBL_SEM_DEFINE(name, initial, limit) \
  struct pbl_sem name = PBL_SEM_INITIALIZER(initial, limit)

/**
 * @brief Initialize a semaphore in dynamically allocated memory.
 *
 * @param[out] s Semaphore.
 * @param initial Initial count, at most @p limit.
 * @param limit Maximum count, at least 1.
 */
void pbl_sem_init(struct pbl_sem *s, uint32_t initial, uint32_t limit);

/**
 * @brief Release a semaphore before its memory is reused.
 *
 * Required for dynamically allocated semaphores.
 *
 * @param s Semaphore.
 */
void pbl_sem_deinit(struct pbl_sem *s);

/**
 * @brief Take a token.
 *
 * @param s Semaphore.
 * @param timeout How long to wait for a token; ignored in an ISR, which never waits.
 * @retval 0 Token taken.
 * @retval -EAGAIN Timed out.
 * @retval -EBUSY No token and @p timeout is @ref PBL_NO_WAIT, or called from an ISR.
 * @retval -EINTR The thread was suspended while waiting.
 */
int pbl_sem_take(struct pbl_sem *s, pbl_timeout_t timeout);
/**
 * @brief Give a token.
 *
 * Wakes the highest priority waiter, or increments the count up to the limit. ISR-safe.
 *
 * @param s Semaphore.
 */
void pbl_sem_give(struct pbl_sem *s);
/**
 * @brief Set the count back to its initial value.
 *
 * Waiters are not woken.
 *
 * @param s Semaphore.
 */
void pbl_sem_reset(struct pbl_sem *s);
/**
 * @brief Get the current count.
 *
 * @param s Semaphore.
 * @return Tokens available.
 */
uint32_t pbl_sem_count(const struct pbl_sem *s);

/** @} */
