/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/types.h>

/**
 * @defgroup kernel_mutex Mutexes
 * @ingroup kernel
 * @brief Recursive mutexes with priority inheritance.
 *
 * The owning thread may lock a mutex again; it is released once every lock has been undone.
 * While a higher priority thread waits, the owner runs at the waiter's priority until it holds no
 * mutex any more. Waiters are served highest priority first, FIFO within a priority, and an
 * unlock hands the mutex directly to the next waiter. Mutexes cannot be used from ISRs.
 *
 * Non-recursive use is expressed with pbl_mutex_assert_held().
 *
 * @code{.c}
 * static PBL_MUTEX_DEFINE(s_lock);
 *
 * int cache_update(const struct entry *e) {
 *   int rc = pbl_mutex_lock(&s_lock, PBL_MSEC(50));
 *   if (rc != 0) {
 *     return rc; // -EAGAIN: still held by another thread after 50 ms
 *   }
 *   prv_store(e);
 *   pbl_mutex_unlock(&s_lock);
 *   return 0;
 * }
 * @endcode
 * @{
 */

/** @brief Recursive mutex. Not usable from ISRs. */
struct pbl_mutex {
  /** Owning thread, NULL when unlocked. */
  struct pbl_thread *owner;
  /** Recursion depth of the owner, 0 when unlocked. */
  uint32_t count;
  /** Return address of the outermost lock, for diagnostics (@c CONFIG_KERNEL_MUTEX_LOCK_LR). */
  uintptr_t lock_lr;
  /** Backend state. */
  struct pbl_mutex_backend backend;
};

/** @brief Static initializer for an unlocked mutex. */
#define PBL_MUTEX_INITIALIZER {.owner = NULL, .count = 0, .lock_lr = 0}

/**
 * @brief Define an unlocked mutex, usable without pbl_mutex_init().
 *
 * @param name Name of the mutex variable.
 */
#define PBL_MUTEX_DEFINE(name) struct pbl_mutex name = PBL_MUTEX_INITIALIZER

/**
 * @brief Initialize a mutex in dynamically allocated memory.
 *
 * @param[out] m Mutex.
 */
void pbl_mutex_init(struct pbl_mutex *m);

/**
 * @brief Release a mutex before its memory is reused.
 *
 * Required for dynamically allocated mutexes. Asserts that the mutex is not held.
 *
 * @param m Mutex.
 */
void pbl_mutex_deinit(struct pbl_mutex *m);

/**
 * @brief Lock a mutex, recording a given lock site.
 *
 * For wrappers that want their own caller recorded in @ref pbl_mutex::lock_lr.
 *
 * @param m Mutex.
 * @param timeout How long to wait for another owner to release it.
 * @param lr Return address to record when this is the outermost lock.
 * @retval 0 Locked.
 * @retval -EAGAIN Timed out.
 * @retval -EBUSY Held by another thread and @p timeout is @ref PBL_NO_WAIT.
 * @retval -EINTR The thread was suspended while waiting.
 */
int pbl_mutex_lock_lr(struct pbl_mutex *m, pbl_timeout_t timeout, uintptr_t lr);

/**
 * @brief Lock a mutex.
 *
 * Succeeds at once if the calling thread already owns it.
 *
 * @param m Mutex.
 * @param timeout How long to wait for another owner to release it.
 * @retval 0 Locked.
 * @retval -EAGAIN Timed out.
 * @retval -EBUSY Held by another thread and @p timeout is @ref PBL_NO_WAIT.
 * @retval -EINTR The thread was suspended while waiting.
 */
static inline int pbl_mutex_lock(struct pbl_mutex *m, pbl_timeout_t timeout) {
  return pbl_mutex_lock_lr(m, timeout, (uintptr_t)PBL_RETURN_ADDRESS(0));
}

/**
 * @brief Undo one lock of a mutex.
 *
 * Asserts that the calling thread owns it. The outermost unlock hands the mutex to the highest
 * priority waiter and drops any inherited priority boost.
 *
 * @param m Mutex.
 */
void pbl_mutex_unlock(struct pbl_mutex *m);

/**
 * @brief Check whether the calling thread owns a mutex.
 *
 * @param m Mutex.
 * @return true if the calling thread holds @p m.
 */
bool pbl_mutex_is_owner(const struct pbl_mutex *m);

/**
 * @brief Assert that ownership of a mutex by the calling thread matches @p held.
 *
 * @param m Mutex.
 * @param held Expected ownership.
 */
void pbl_mutex_assert_held(const struct pbl_mutex *m, bool held);

/** @} */
