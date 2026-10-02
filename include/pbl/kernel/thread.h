/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/drivers/mpu.h"
#include "pbl/kernel/types.h"
#include "pbl/kernel/compiler.h"

/**
 * @defgroup kernel_thread Threads
 * @ingroup kernel
 * @brief Thread creation, lifecycle and per-thread state.
 *
 * Threads are scheduled by fixed priority, highest first; threads of equal priority round-robin
 * on every tick and on pbl_thread_yield(). The thread struct and its stack are owned by the
 * caller and must outlive the thread. Returning from the entry function ends the thread, after
 * which the struct may be reused for a new one.
 *
 * @code{.c}
 * PBL_THREAD_STACK_DEFINE(s_worker_stack, 1024);
 * static struct pbl_thread s_worker;
 *
 * static void prv_worker(void *arg) {
 *   for (;;) {
 *     do_work(arg);
 *     pbl_thread_sleep(PBL_MSEC(500));
 *   }
 * }
 *
 * void worker_start(void) {
 *   const struct pbl_thread_attr attr = {
 *     .name = "Worker",
 *     .entry = prv_worker,
 *     .prio = 2,
 *     .privileged = true,
 *     .stack = s_worker_stack,
 *     .stack_size = sizeof(s_worker_stack),
 *   };
 *   pbl_thread_create(&s_worker, &attr);
 * }
 * @endcode
 * @{
 */

/** @brief Size of a thread name, including the terminating NUL. */
#define PBL_THREAD_NAME_LEN 16
/** @brief Number of MPU regions switched in with each thread. */
#define PBL_THREAD_MAX_MEM_REGIONS 4

/**
 * @brief Thread entry function.
 *
 * @param arg Argument given in @ref pbl_thread_attr::arg.
 */
typedef void (*pbl_thread_entry_t)(void *arg);

/** @brief Thread state. */
enum pbl_thread_state {
  /** Runnable, waiting for the CPU. */
  PBL_THREAD_READY,
  /** Currently executing. */
  PBL_THREAD_RUNNING,
  /** Waiting on an object or sleeping. */
  PBL_THREAD_BLOCKED,
  /** Stopped by pbl_thread_suspend() until pbl_thread_resume(). */
  PBL_THREAD_SUSPENDED,
  /** Ended or aborted; the struct may be reused. */
  PBL_THREAD_DEAD,
};

/** @brief Thread creation attributes. */
struct pbl_thread_attr {
  /** Name, truncated to @ref PBL_THREAD_NAME_LEN - 1 characters. */
  const char *name;
  /** Entry function. */
  pbl_thread_entry_t entry;
  /** Argument passed to @ref entry. */
  void *arg;
  /** Priority, @ref PBL_PRIO_IDLE to @ref PBL_PRIO_MAX. */
  pbl_prio_t prio;
  /** Run in privileged mode; unprivileged threads are confined by the MPU. */
  bool privileged;
  /** Lowest address of the stack; the caller owns the memory. */
  void *stack;
  /** Stack size in bytes, at least 128. */
  size_t stack_size;
  /** MPU regions switched in with the thread. NULL entries are ignored. */
  const MpuRegion *regions[PBL_THREAD_MAX_MEM_REGIONS];
};

/**
 * @brief Thread object.
 *
 * Caller-owned; set up by pbl_thread_create(). Read fields through the accessor functions.
 */
struct pbl_thread {
  /** Backend state; first, the arch code relies on its offset. */
  struct pbl_thread_backend backend;
  /** Unique per creation, never 0. */
  uint32_t id;
  /** NUL-terminated name. */
  char name[PBL_THREAD_NAME_LEN];
  /** Effective priority, including any boost inherited through a mutex. */
  pbl_prio_t prio;
  /** Runs in privileged mode. */
  bool privileged;
  /** Lowest address of the stack. */
  void *stack;
  /** Stack size in bytes. */
  size_t stack_size;
  /** Thread-local pointer slots, see pbl_thread_tls_get(). */
  void *tls[CONFIG_KERNEL_THREAD_TLS_SLOTS];
};

/**
 * @brief Define a thread stack with the alignment the MPU port needs for a guard region.
 *
 * Expands to a @c static array; @c CONFIG_KERNEL_STACK_ALIGN gives the alignment.
 *
 * @param name Name of the stack array.
 * @param size Stack size in bytes.
 */
#define PBL_THREAD_STACK_DEFINE(name, size) \
  static uint8_t name[size] PBL_ALIGNED(CONFIG_KERNEL_STACK_ALIGN)

/**
 * @brief Create and start a thread.
 *
 * The stack is filled with a pattern for high-water tracking. The new thread runs as soon as it
 * is the highest priority runnable thread, possibly before this call returns. Returning from the
 * entry function ends the thread. Asserts if @p t still holds a live thread.
 *
 * @param[out] t Thread object, caller-owned.
 * @param attr Creation attributes; only read during the call.
 * @return 0.
 */
int pbl_thread_create(struct pbl_thread *t, const struct pbl_thread_attr *attr);
/**
 * @brief End a thread.
 *
 * Mutexes it holds are not released.
 *
 * @param t Thread to end, or NULL for the calling thread, in which case this does not return.
 */
void pbl_thread_abort(struct pbl_thread *t);
/**
 * @brief Stop scheduling a thread until pbl_thread_resume().
 *
 * A thread suspended while blocked sees the blocking call fail with @c -EINTR once resumed.
 * Has no effect on a suspended or dead thread.
 *
 * @param t Thread to suspend, or NULL for the calling thread.
 */
void pbl_thread_suspend(struct pbl_thread *t);
/**
 * @brief Make a suspended thread runnable again.
 *
 * Has no effect if @p t is not suspended.
 *
 * @param t Thread to resume.
 */
void pbl_thread_resume(struct pbl_thread *t);
/** @brief Let other ready threads of the same priority run. */
void pbl_thread_yield(void);
/**
 * @brief Block the calling thread for a while.
 *
 * @param timeout Time to sleep; @ref PBL_NO_WAIT yields instead.
 */
void pbl_thread_sleep(pbl_timeout_t timeout);

/**
 * @brief Get the calling thread.
 *
 * Callable from unprivileged code.
 *
 * @return The running thread, or the interrupted thread when called from an ISR.
 */
struct pbl_thread *pbl_thread_current(void);
/**
 * @brief Get the kernel's idle thread.
 *
 * @return The idle thread, which runs at @ref PBL_PRIO_IDLE when nothing else is runnable.
 */
struct pbl_thread *pbl_thread_idle(void);

/**
 * @brief Change the base priority of a thread.
 *
 * A priority boost inherited through a mutex is kept until the mutex is released.
 *
 * @param t Thread.
 * @param prio New priority, at most @ref PBL_PRIO_MAX.
 */
void pbl_thread_prio_set(struct pbl_thread *t, pbl_prio_t prio);
/**
 * @brief Get the base priority of a thread.
 *
 * @param t Thread.
 * @return Priority set at creation or by pbl_thread_prio_set(), without inherited boosts.
 */
pbl_prio_t pbl_thread_prio_get(const struct pbl_thread *t);
/**
 * @brief Get the state of a thread.
 *
 * @param t Thread.
 * @return Current state.
 */
enum pbl_thread_state pbl_thread_state(const struct pbl_thread *t);

/**
 * @brief Get the name of a thread.
 *
 * @param t Thread.
 * @return NUL-terminated name, owned by @p t.
 */
static inline const char *pbl_thread_name(const struct pbl_thread *t) {
  return t->name;
}

/**
 * @brief Get the creation ID of a thread.
 *
 * Distinguishes successive threads created in the same struct.
 *
 * @param t Thread.
 * @return ID, unique per creation and never 0.
 */
static inline uint32_t pbl_thread_id(const struct pbl_thread *t) {
  return t->id;
}

/**
 * @brief Replace the MPU regions of a thread.
 *
 * Used for the idle thread, whose regions cannot be passed at creation.
 *
 * @param t Thread.
 * @param regions Array of @ref PBL_THREAD_MAX_MEM_REGIONS regions; NULL entries are ignored.
 */
void pbl_thread_regions_set(struct pbl_thread *t, const MpuRegion *const *regions);

/**
 * @brief Read a thread-local pointer slot.
 *
 * @param t Thread.
 * @param slot Slot index, below @c CONFIG_KERNEL_THREAD_TLS_SLOTS.
 * @return Stored pointer, NULL if never set.
 */
static inline void *pbl_thread_tls_get(const struct pbl_thread *t, unsigned int slot) {
  return t->tls[slot];
}
/**
 * @brief Write a thread-local pointer slot.
 *
 * @param t Thread.
 * @param slot Slot index, below @c CONFIG_KERNEL_THREAD_TLS_SLOTS.
 * @param v Pointer to store.
 */
static inline void pbl_thread_tls_set(struct pbl_thread *t, unsigned int slot, void *v) {
  t->tls[slot] = v;
}

/**
 * @brief Handle a thread that overran its stack.
 *
 * Implemented by the application.
 *
 * @param t Thread that overflowed.
 * @param name Its name.
 */
void pbl_thread_stack_overflow(struct pbl_thread *t, const char *name);

/**
 * @brief Decide whether code may raise privilege through the kernel's supervisor call.
 *
 * Implemented by the application. Called from the SVC handler on every privilege-raise request.
 *
 * @param caller_pc Address of the instruction after the SVC.
 * @return true to grant privileged mode to the caller.
 */
bool pbl_kernel_privilege_raise_allowed(uint32_t caller_pc);

/**
 * @brief Provide a dedicated privileged stack for the current thread's syscalls.
 *
 * Implemented by the application; the kernel's weak default returns NULL. Called from the SVC
 * handler once a privilege raise is allowed: the exception frame and the caller's stacked
 * arguments are copied onto the returned stack, so the syscall body runs there.
 *
 * @param[out] base_out Lowest address of the stack, used as the stack limit where supported.
 * @return Top of the stack, or NULL to run syscalls on the caller's stack.
 */
uint32_t *pbl_kernel_syscall_stack(uintptr_t *base_out);

/**
 * @brief Notify the application that privilege is being raised for a syscall.
 *
 * Implemented by the application. Called from the SVC handler just before the thread is made
 * privileged.
 *
 * @param orig_sp Caller's stack pointer before the SVC.
 * @param[in,out] lr_slot Stacked return address of the syscall; may be rewritten to redirect the
 * return, e.g. through code that drops privilege again.
 */
void pbl_kernel_syscall_entered(uintptr_t orig_sp, uintptr_t *lr_slot);

/** @} */
