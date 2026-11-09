/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/delay.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/logging/logging.h>

#define CAPACITY           4
#define ITEMS_PER_PRODUCER 50
#define NUM_PRODUCERS      2
#define NUM_CONSUMERS      2

/*
 * What a message queue does inside: one semaphore counts the free slots, another the items, and
 * a mutex guards the indices. Prefer pbl_msgq, which also works from interrupts.
 */
struct buffer {
  unsigned int items[CAPACITY];
  unsigned int head;
  unsigned int tail;
  unsigned int used;
  unsigned int max_used;
  struct pbl_sem free_slots;
  struct pbl_sem queued_items;
  struct pbl_mutex lock;
};

static struct buffer s_buffer = {
  .free_slots = PBL_SEM_INITIALIZER(CAPACITY, CAPACITY),
  .queued_items = PBL_SEM_INITIALIZER(0, CAPACITY),
  .lock = PBL_MUTEX_INITIALIZER,
};

static unsigned int s_sums[NUM_CONSUMERS];

PBL_THREAD_STACK_DEFINE(s_stack_0, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_1, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_2, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_3, 1024);
static struct pbl_thread s_threads[NUM_PRODUCERS + NUM_CONSUMERS];

static void prv_put(struct buffer *buffer, unsigned int item) {
  /* Waits for a free slot before the lock, never with it. */
  pbl_sem_take(&buffer->free_slots, PBL_FOREVER);

  pbl_mutex_lock(&buffer->lock, PBL_FOREVER);
  buffer->items[buffer->tail] = item;
  buffer->tail = (buffer->tail + 1) % CAPACITY;
  buffer->used++;
  if (buffer->used > buffer->max_used) {
    buffer->max_used = buffer->used;
  }
  pbl_mutex_unlock(&buffer->lock);

  pbl_sem_give(&buffer->queued_items);
}

static unsigned int prv_get(struct buffer *buffer) {
  unsigned int item;

  pbl_sem_take(&buffer->queued_items, PBL_FOREVER);

  pbl_mutex_lock(&buffer->lock, PBL_FOREVER);
  item = buffer->items[buffer->head];
  buffer->head = (buffer->head + 1) % CAPACITY;
  buffer->used--;
  pbl_mutex_unlock(&buffer->lock);

  pbl_sem_give(&buffer->free_slots);

  return item;
}

static void prv_producer(void *arg) {
  for (unsigned int item = 1; item <= ITEMS_PER_PRODUCER; item++) {
    prv_put(&s_buffer, item);
  }
}

static void prv_consumer(void *arg) {
  unsigned int *sum = arg;

  for (unsigned int i = 0; i < ITEMS_PER_PRODUCER * NUM_PRODUCERS / NUM_CONSUMERS; i++) {
    *sum += prv_get(&s_buffer);
    /* Slower than the producers, so the buffer fills up. */
    pbl_thread_yield();
  }
}

int main(void) {
  void *const stacks[] = {s_stack_0, s_stack_1, s_stack_2, s_stack_3};
  const unsigned int expected = NUM_PRODUCERS * (ITEMS_PER_PRODUCER * (ITEMS_PER_PRODUCER + 1) / 2);
  unsigned int sum = 0;
  int ret;

  for (unsigned int i = 0; i < NUM_PRODUCERS + NUM_CONSUMERS; i++) {
    const bool producer = i < NUM_PRODUCERS;
    const struct pbl_thread_attr attr = {
      .name = producer ? "producer" : "consumer",
      .entry = producer ? prv_producer : prv_consumer,
      .arg = producer ? NULL : &s_sums[i - NUM_PRODUCERS],
      .prio = CONFIG_KERNEL_MAIN_THREAD_PRIO - 1,
      .privileged = true,
      .stack = stacks[i],
      .stack_size = sizeof(s_stack_0),
    };

    pbl_thread_create(&s_threads[i], &attr);
  }

  for (unsigned int i = 0; i < NUM_PRODUCERS + NUM_CONSUMERS; i++) {
    ret = PBL_POLL_SLEEP_MS(pbl_thread_state(&s_threads[i]) == PBL_THREAD_DEAD, 1, 1000);
    if (ret != 0) {
      PBL_LOG_ERR("main: %s did not end (%d)", i < NUM_PRODUCERS ? "a producer" : "a consumer",
                  ret);
      return ret;
    }
  }

  for (unsigned int i = 0; i < NUM_CONSUMERS; i++) {
    sum += s_sums[i];
  }

  PBL_LOG_INFO("buffer: %u items produced, consumed items add up to %u of %u",
               NUM_PRODUCERS * ITEMS_PER_PRODUCER, sum, expected);
  PBL_LOG_INFO("buffer: held at most %u of %u items, %u left", s_buffer.max_used, CAPACITY,
               s_buffer.used);

  PBL_LOG_INFO("Sample done");

  return 0;
}
