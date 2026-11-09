/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/delay.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/logging/logging.h>

#define NUM_CHAIRS    3
#define NUM_CUSTOMERS 5

/* Customers run above the barber, so all of them arrive before the first haircut starts. */
#define CUSTOMER_PRIO (CONFIG_KERNEL_MAIN_THREAD_PRIO - 1)
#define BARBER_PRIO   (CONFIG_KERNEL_MAIN_THREAD_PRIO - 2)

struct customer {
  unsigned int id;
  struct pbl_thread thread;
};

/* Counts the customers waiting, which is what the barber sleeps on. */
static PBL_SEM_DEFINE(s_customers, 0, NUM_CHAIRS);
/* Given by the barber when he is ready for the next one. */
static PBL_SEM_DEFINE(s_barber_chair, 0, 1);
static PBL_MUTEX_DEFINE(s_shop_lock);

static unsigned int s_waiting;
static unsigned int s_haircuts;
static unsigned int s_turned_away;

static struct customer s_customers_list[NUM_CUSTOMERS];
static struct pbl_thread s_barber;

PBL_THREAD_STACK_DEFINE(s_barber_stack, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_0, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_1, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_2, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_3, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_4, 1024);

static void prv_barber(void *arg) {
  for (;;) {
    /* Asleep until a customer sits down in the waiting room. */
    pbl_sem_take(&s_customers, PBL_FOREVER);

    pbl_mutex_lock(&s_shop_lock, PBL_FOREVER);
    s_waiting--;
    pbl_mutex_unlock(&s_shop_lock);

    pbl_sem_give(&s_barber_chair);
    pbl_sleep_ms(20);

    pbl_mutex_lock(&s_shop_lock, PBL_FOREVER);
    s_haircuts++;
    pbl_mutex_unlock(&s_shop_lock);
  }
}

static void prv_customer(void *arg) {
  const struct customer *customer = arg;
  bool seated;

  /* Looking for a chair and taking it is one step, or two customers could take the last one. */
  pbl_mutex_lock(&s_shop_lock, PBL_FOREVER);
  seated = s_waiting < NUM_CHAIRS;
  if (seated) {
    s_waiting++;
  } else {
    s_turned_away++;
  }
  pbl_mutex_unlock(&s_shop_lock);

  if (!seated) {
    PBL_LOG_INFO("customer %u: no free chair, leaves", customer->id);
    return;
  }

  pbl_sem_give(&s_customers);
  pbl_sem_take(&s_barber_chair, PBL_FOREVER);
  PBL_LOG_INFO("customer %u: in the barber chair", customer->id);
}

static void prv_start(struct pbl_thread *t, const char *name, pbl_thread_entry_t entry, void *arg,
                      pbl_prio_t prio, void *stack) {
  const struct pbl_thread_attr attr = {
    .name = name,
    .entry = entry,
    .arg = arg,
    .prio = prio,
    .privileged = true,
    .stack = stack,
    .stack_size = sizeof(s_barber_stack),
  };

  pbl_thread_create(t, &attr);
}

int main(void) {
  void *const stacks[NUM_CUSTOMERS] = {s_stack_0, s_stack_1, s_stack_2, s_stack_3, s_stack_4};
  int ret;

  prv_start(&s_barber, "barber", prv_barber, NULL, BARBER_PRIO, s_barber_stack);
  pbl_sleep_ms(10);
  PBL_LOG_INFO("shop: empty, barber asleep: %s",
               pbl_thread_state(&s_barber) == PBL_THREAD_BLOCKED ? "yes" : "no");

  PBL_LOG_INFO("shop: %u customers arrive at once, for %u chairs", NUM_CUSTOMERS, NUM_CHAIRS);
  for (unsigned int i = 0; i < NUM_CUSTOMERS; i++) {
    s_customers_list[i].id = i;
    prv_start(&s_customers_list[i].thread, "customer", prv_customer, &s_customers_list[i],
              CUSTOMER_PRIO, stacks[i]);
  }

  ret = PBL_POLL_SLEEP_MS(s_haircuts + s_turned_away == NUM_CUSTOMERS, 5, 1000);
  if (ret != 0) {
    PBL_LOG_ERR("shop: customers still waiting (%d)", ret);
    return ret;
  }

  pbl_sleep_ms(10);
  PBL_LOG_INFO("shop: %u haircuts, %u customers turned away", s_haircuts, s_turned_away);
  PBL_LOG_INFO("shop: empty, barber asleep: %s",
               pbl_thread_state(&s_barber) == PBL_THREAD_BLOCKED ? "yes" : "no");

  PBL_LOG_INFO("Sample done");

  return 0;
}
