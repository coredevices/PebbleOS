/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/delay.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/logging/logging.h>

#define NUM_READERS 3
#define READS       10
#define WRITES      5

/*
 * Any number of readers, or one writer. The room is a semaphore and not a mutex because the
 * reader that leaves it last is not always the one that entered first.
 */
struct rwlock {
  struct pbl_mutex readers_lock;
  unsigned int readers;
  struct pbl_sem room;
};

struct record {
  unsigned int version;
  unsigned int copy_of_version;
};

static struct rwlock s_lock = {
  .readers_lock = PBL_MUTEX_INITIALIZER,
  .room = PBL_SEM_INITIALIZER(1, 1),
};
static struct record s_record;

/* Readers share the room, so what they count needs a lock of its own. */
static PBL_MUTEX_DEFINE(s_stats_lock);
static unsigned int s_reading;
static unsigned int s_max_reading;
static unsigned int s_torn_reads;
static unsigned int s_writes_with_readers;

PBL_THREAD_STACK_DEFINE(s_stack_0, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_1, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_2, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_3, 1024);
static struct pbl_thread s_threads[NUM_READERS + 1];

static void prv_read_lock(struct rwlock *lock) {
  pbl_mutex_lock(&lock->readers_lock, PBL_FOREVER);
  if (++lock->readers == 1) {
    /* The first reader in takes the room for all of them. */
    pbl_sem_take(&lock->room, PBL_FOREVER);
  }
  pbl_mutex_unlock(&lock->readers_lock);
}

static void prv_read_unlock(struct rwlock *lock) {
  pbl_mutex_lock(&lock->readers_lock, PBL_FOREVER);
  if (--lock->readers == 0) {
    pbl_sem_give(&lock->room);
  }
  pbl_mutex_unlock(&lock->readers_lock);
}

static void prv_write_lock(struct rwlock *lock) {
  pbl_sem_take(&lock->room, PBL_FOREVER);
}

static void prv_write_unlock(struct rwlock *lock) {
  pbl_sem_give(&lock->room);
}

static void prv_reader(void *arg) {
  for (unsigned int i = 0; i < READS; i++) {
    prv_read_lock(&s_lock);

    pbl_mutex_lock(&s_stats_lock, PBL_FOREVER);
    s_reading++;
    if (s_reading > s_max_reading) {
      s_max_reading = s_reading;
    }
    pbl_mutex_unlock(&s_stats_lock);

    unsigned int version = s_record.version;
    pbl_sleep_ms(2);
    const bool torn = version != s_record.copy_of_version;

    pbl_mutex_lock(&s_stats_lock, PBL_FOREVER);
    if (torn) {
      s_torn_reads++;
    }
    s_reading--;
    pbl_mutex_unlock(&s_stats_lock);

    prv_read_unlock(&s_lock);

    /* Out of the room for a while, or a writer would never find it empty. */
    pbl_sleep_ms(3);
  }
}

static void prv_writer(void *arg) {
  for (unsigned int i = 0; i < WRITES; i++) {
    prv_write_lock(&s_lock);

    pbl_mutex_lock(&s_stats_lock, PBL_FOREVER);
    if (s_reading != 0) {
      s_writes_with_readers++;
    }
    pbl_mutex_unlock(&s_stats_lock);

    /* Half written for a while, which no reader may see. */
    s_record.version++;
    pbl_sleep_ms(2);
    s_record.copy_of_version = s_record.version;

    prv_write_unlock(&s_lock);
    pbl_sleep_ms(5);
  }
}

int main(void) {
  void *const stacks[] = {s_stack_0, s_stack_1, s_stack_2, s_stack_3};
  int ret;

  for (unsigned int i = 0; i < NUM_READERS + 1; i++) {
    const bool reader = i < NUM_READERS;
    const struct pbl_thread_attr attr = {
      .name = reader ? "reader" : "writer",
      .entry = reader ? prv_reader : prv_writer,
      .prio = CONFIG_KERNEL_MAIN_THREAD_PRIO - 1,
      .privileged = true,
      .stack = stacks[i],
      .stack_size = sizeof(s_stack_0),
    };

    pbl_thread_create(&s_threads[i], &attr);
  }

  for (unsigned int i = 0; i < NUM_READERS + 1; i++) {
    ret = PBL_POLL_SLEEP_MS(pbl_thread_state(&s_threads[i]) == PBL_THREAD_DEAD, 1, 2000);
    if (ret != 0) {
      PBL_LOG_ERR("main: %s did not end (%d)", i < NUM_READERS ? "a reader" : "the writer", ret);
      return ret;
    }
  }

  PBL_LOG_INFO("record: version %u after %u writes", s_record.version, WRITES);
  PBL_LOG_INFO("record: up to %u readers at once, %u half written records read", s_max_reading,
               s_torn_reads);
  PBL_LOG_INFO("record: %u writes made with a reader in the room", s_writes_with_readers);

  PBL_LOG_INFO("Sample done");

  return 0;
}
