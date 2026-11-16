/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/irq.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sched.h>

#include <sys/lock.h>

struct __lock {
  struct pbl_mutex mutex;
};

#ifdef CONFIG_LIBC_PICOLIBC
struct __lock __lock___libc_recursive_mutex = {PBL_MUTEX_INITIALIZER};
#else
struct __lock __lock___arc4random_mutex = {PBL_MUTEX_INITIALIZER};
struct __lock __lock___at_quick_exit_mutex = {PBL_MUTEX_INITIALIZER};
struct __lock __lock___atexit_recursive_mutex = {PBL_MUTEX_INITIALIZER};
struct __lock __lock___dd_hash_mutex = {PBL_MUTEX_INITIALIZER};
struct __lock __lock___env_recursive_mutex = {PBL_MUTEX_INITIALIZER};
struct __lock __lock___malloc_recursive_mutex = {PBL_MUTEX_INITIALIZER};
struct __lock __lock___sfp_recursive_mutex = {PBL_MUTEX_INITIALIZER};
struct __lock __lock___tz_mutex = {PBL_MUTEX_INITIALIZER};
#endif

static bool prv_can_lock(_LOCK_T lock) {
  return lock != nullptr && !(pbl_in_isr() || pbl_irq_is_locked() || !pbl_kernel_is_running() ||
                              pbl_sched_is_locked());
}

void __retarget_lock_init(_LOCK_T *lock) {
  *lock = pbl_malloc(sizeof(**lock));
  if (*lock != nullptr) {
    pbl_mutex_init(&(*lock)->mutex);
  }
}

void __retarget_lock_init_recursive(_LOCK_T *lock) {
  __retarget_lock_init(lock);
}

void __retarget_lock_close(_LOCK_T lock) {
  if (lock != nullptr) {
    pbl_mutex_deinit(&lock->mutex);
    pbl_free(lock);
  }
}

void __retarget_lock_close_recursive(_LOCK_T lock) {
  __retarget_lock_close(lock);
}

void __retarget_lock_acquire(_LOCK_T lock) {
  if (prv_can_lock(lock)) {
    pbl_mutex_lock(&lock->mutex, PBL_FOREVER);
  }
}

void __retarget_lock_acquire_recursive(_LOCK_T lock) {
  __retarget_lock_acquire(lock);
}

int __retarget_lock_try_acquire(_LOCK_T lock) {
  if (!prv_can_lock(lock)) {
    return 0;
  }

  return pbl_mutex_lock(&lock->mutex, PBL_NO_WAIT) == 0 ? 0 : -1;
}

int __retarget_lock_try_acquire_recursive(_LOCK_T lock) {
  return __retarget_lock_try_acquire(lock);
}

void __retarget_lock_release(_LOCK_T lock) {
  if (prv_can_lock(lock)) {
    pbl_mutex_unlock(&lock->mutex);
  }
}

void __retarget_lock_release_recursive(_LOCK_T lock) {
  __retarget_lock_release(lock);
}
