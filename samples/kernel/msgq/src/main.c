/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>

#include <pbl/kernel/msgq.h>
#include <pbl/kernel/thread.h>
#include <pbl/logging/logging.h>

#define QUEUE_LEN   2
#define NUM_SAMPLES 5

struct sample {
  uint32_t seq;
  int32_t value;
};

/* Messages are copied in and out, so the queue owns all the storage it needs. */
static PBL_MSGQ_DEFINE(s_samples, sizeof(struct sample), QUEUE_LEN);

PBL_THREAD_STACK_DEFINE(s_producer_stack, 1024);
static struct pbl_thread s_producer;

static void prv_producer(void *arg) {
  for (uint32_t seq = 0; seq < NUM_SAMPLES; seq++) {
    const struct sample sample = {.seq = seq, .value = (int32_t)seq * 10};

    /* Blocks while the queue is full: a slow consumer slows the producer down. */
    int ret = pbl_msgq_put(&s_samples, &sample, PBL_FOREVER);
    if (ret != 0) {
      PBL_LOG_ERR("producer: put failed (%d)", ret);
      return;
    }
  }
}

static int prv_consume(void) {
  uint32_t received = 0;
  struct sample sample;
  int ret;

  for (;;) {
    ret = pbl_msgq_get(&s_samples, &sample, PBL_MSEC(100));
    if (ret == -EAGAIN) {
      break;
    } else if (ret != 0) {
      return ret;
    }

    PBL_LOG_INFO("consumer: sample %" PRIu32 " = %" PRId32, sample.seq, sample.value);
    received++;
  }

  PBL_LOG_INFO("consumer: no sample for 100 ms, %" PRIu32 " received", received);

  return received == NUM_SAMPLES ? 0 : -EIO;
}

static int prv_jump_the_queue(void) {
  const struct sample routine = {.seq = 100};
  const struct sample urgent = {.seq = 999};
  struct sample sample;
  int ret;

  ret = pbl_msgq_put(&s_samples, &routine, PBL_NO_WAIT);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_msgq_put_front(&s_samples, &urgent, PBL_NO_WAIT);
  if (ret != 0) {
    return ret;
  }

  /* The queue is full: without waiting, a third message is refused. */
  ret = pbl_msgq_put(&s_samples, &routine, PBL_NO_WAIT);
  PBL_LOG_INFO("main: put on a full queue returns %s", ret == -EBUSY ? "-EBUSY" : "something else");

  ret = pbl_msgq_get(&s_samples, &sample, PBL_NO_WAIT);
  if (ret != 0) {
    return ret;
  }

  PBL_LOG_INFO("main: sample %" PRIu32 " was put last and comes out first", sample.seq);
  pbl_msgq_purge(&s_samples);

  return 0;
}

int main(void) {
  const struct pbl_thread_attr attr = {
    .name = "producer",
    .entry = prv_producer,
    .prio = CONFIG_KERNEL_MAIN_THREAD_PRIO - 1,
    .privileged = true,
    .stack = s_producer_stack,
    .stack_size = sizeof(s_producer_stack),
  };
  struct sample sample;
  int ret;

  pbl_thread_create(&s_producer, &attr);

  /* Let the producer run until the queue stops it. */
  pbl_thread_sleep(PBL_MSEC(50));
  PBL_LOG_INFO("main: producer is blocked with %" PRIu32 " of %u samples queued",
               pbl_msgq_num_used(&s_samples), NUM_SAMPLES);

  ret = pbl_msgq_peek(&s_samples, &sample);
  if (ret != 0) {
    PBL_LOG_ERR("main: peek failed (%d)", ret);
    return ret;
  }
  PBL_LOG_INFO("main: sample %" PRIu32 " is next, and still queued", sample.seq);

  ret = prv_consume();
  if (ret != 0) {
    PBL_LOG_ERR("main: consuming failed (%d)", ret);
    return ret;
  }

  ret = prv_jump_the_queue();
  if (ret != 0) {
    PBL_LOG_ERR("main: jumping the queue failed (%d)", ret);
    return ret;
  }

  PBL_LOG_INFO("Sample done");

  return 0;
}
