/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/logging/logging.h>

#define DEPOSITS    100
#define WORKER_PRIO (CONFIG_KERNEL_MAIN_THREAD_PRIO - 1)

struct account {
  struct pbl_mutex lock;
  bool use_lock;
  unsigned int balance;
};

static struct account s_account = {.lock = PBL_MUTEX_INITIALIZER};
static PBL_SEM_DEFINE(s_holder_release, 0, 1);

PBL_THREAD_STACK_DEFINE(s_stack_a, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_b, 1024);
static struct pbl_thread s_thread_a;
static struct pbl_thread s_thread_b;

static void prv_deposit(struct account *account) {
  if (account->use_lock) {
    pbl_mutex_lock(&account->lock, PBL_FOREVER);
  }

  unsigned int balance = account->balance;
  /* Stands for being preempted between the read and the write. */
  pbl_thread_yield();
  account->balance = balance + 1;

  if (account->use_lock) {
    pbl_mutex_unlock(&account->lock);
  }
}

static void prv_depositor(void *arg) {
  for (unsigned int i = 0; i < DEPOSITS; i++) {
    prv_deposit(arg);
  }
}

static void prv_holder(void *arg) {
  struct account *account = arg;

  pbl_mutex_lock(&account->lock, PBL_FOREVER);
  pbl_sem_take(&s_holder_release, PBL_FOREVER);
  pbl_mutex_unlock(&account->lock);
}

static void prv_start(struct pbl_thread *t, const char *name, pbl_thread_entry_t entry, void *stack,
                      size_t stack_size) {
  const struct pbl_thread_attr attr = {
    .name = name,
    .entry = entry,
    .arg = &s_account,
    .prio = WORKER_PRIO,
    .privileged = true,
    .stack = stack,
    .stack_size = stack_size,
  };

  pbl_thread_create(t, &attr);
}

static int prv_join(const struct pbl_thread *t) {
  return PBL_POLL_SLEEP_MS(pbl_thread_state(t) == PBL_THREAD_DEAD, 1, 1000);
}

static int prv_run_depositors(bool use_lock) {
  int ret;

  s_account.balance = 0;
  s_account.use_lock = use_lock;

  prv_start(&s_thread_a, "a", prv_depositor, s_stack_a, sizeof(s_stack_a));
  prv_start(&s_thread_b, "b", prv_depositor, s_stack_b, sizeof(s_stack_b));

  ret = prv_join(&s_thread_a);
  if (ret == 0) {
    ret = prv_join(&s_thread_b);
  }
  if (ret != 0) {
    PBL_LOG_ERR("main: depositors did not end (%d)", ret);
    return ret;
  }

  PBL_LOG_INFO("main: %s the lock, balance is %u after %u deposits", use_lock ? "with" : "without",
               s_account.balance, 2 * DEPOSITS);

  return 0;
}

int main(void) {
  int ret;

  ret = prv_run_depositors(false);
  if (ret != 0) {
    return ret;
  }

  ret = prv_run_depositors(true);
  if (ret != 0) {
    return ret;
  }

  /* A lock held elsewhere for long is better waited for with a limit. */
  prv_start(&s_thread_a, "holder", prv_holder, s_stack_a, sizeof(s_stack_a));
  pbl_sleep_ms(10);

  ret = pbl_mutex_lock(&s_account.lock, PBL_MSEC(10));
  PBL_LOG_INFO("main: lock held by another thread, 10 ms wait returns %s",
               ret == -EAGAIN ? "-EAGAIN" : "something else");

  pbl_sem_give(&s_holder_release);
  ret = pbl_mutex_lock(&s_account.lock, PBL_FOREVER);
  if (ret != 0) {
    PBL_LOG_ERR("main: cannot take the released lock (%d)", ret);
    return ret;
  }

  /* The owner may lock again, e.g. in a function called with the lock held. */
  pbl_mutex_lock(&s_account.lock, PBL_FOREVER);
  pbl_mutex_unlock(&s_account.lock);
  PBL_LOG_INFO("main: locked twice, unlocked once, still the owner: %s",
               pbl_mutex_is_owner(&s_account.lock) ? "yes" : "no");
  pbl_mutex_unlock(&s_account.lock);

  PBL_LOG_INFO("Sample done");

  return 0;
}
