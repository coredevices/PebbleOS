/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/timer.h>

#include <pbl_arch_posix.h>
#include <posix_host.h>
#include <pthread.h>
#include <sys/time.h>

#define HOST_HZ 1000000U

// Host monotonic time of the pending event, UINT64_MAX if none.
static uint64_t s_deadline_us = UINT64_MAX;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;

static int prv_cs_init(const struct pbl_device *dev);
static int prv_ce_init(const struct pbl_device *dev);

static uint32_t prv_read(const struct pbl_clocksource *cs) {
  return (uint32_t)posix_host_monotonic_us();
}

static const struct pbl_clocksource_ops s_cs_ops = {
  .read = prv_read,
};

PBL_DEVICE_STATE_DEFINE(s_host_cs);
static const struct pbl_clocksource s_host_cs =
    PBL_CLOCKSOURCE_INIT(s_host_cs, "host", prv_cs_init, &s_cs_ops, 32);

const struct pbl_clocksource *const pbl_system_clocksource = &s_host_cs;

#ifndef CONFIG_ARCH_POSIX_FIBERS
static pthread_cond_t s_cond = PTHREAD_COND_INITIALIZER;
#endif

static void prv_set_deadline(uint64_t deadline_us) {
  pthread_mutex_lock(&s_lock);
  s_deadline_us = deadline_us;
#ifndef CONFIG_ARCH_POSIX_FIBERS
  pthread_cond_signal(&s_cond);
#endif
  pthread_mutex_unlock(&s_lock);
}

static void prv_set_next(const struct pbl_clockevent *ce, uint32_t delta) {
  prv_set_deadline(posix_host_monotonic_us() + delta);
}

static void prv_shutdown(const struct pbl_clockevent *ce) {
  prv_set_deadline(UINT64_MAX);
}

static const struct pbl_clockevent_ops s_ce_ops = {
  .set_next = prv_set_next,
  .shutdown = prv_shutdown,
};

PBL_DEVICE_STATE_DEFINE(s_host_ce);
static const struct pbl_clockevent s_host_ce =
    PBL_CLOCKEVENT_INIT(s_host_ce, "host", prv_ce_init, &s_ce_ops, 1U, UINT32_MAX);

const struct pbl_clockevent *const pbl_system_clockevent = &s_host_ce;

static void prv_isr(void *arg) {
  pbl_clockevent_handler(&s_host_ce);
}

// Takes the pending event if it is due.
static bool prv_take_due(void) {
  bool due;

  pthread_mutex_lock(&s_lock);
  due = s_deadline_us <= posix_host_monotonic_us();
  if (due) {
    s_deadline_us = UINT64_MAX;
  }
  pthread_mutex_unlock(&s_lock);

  return due;
}

#ifdef CONFIG_ARCH_POSIX_FIBERS
static void prv_poll(void) {
  if (prv_take_due()) {
    pbl_posix_irq_run(prv_isr, nullptr);
  }
}

POSIX_HOST_HOOK(.poll = prv_poll)

static void prv_start(void) {
}
#else
// The condition variable runs on the realtime clock.
static struct timespec prv_realtime_in(uint64_t us) {
  struct timeval now;
  gettimeofday(&now, nullptr);
  uint64_t ns = (uint64_t)now.tv_usec * 1000U + us * 1000U;

  return (struct timespec){
    .tv_sec = now.tv_sec + (time_t)(ns / 1000000000U),
    .tv_nsec = (long)(ns % 1000000000U),
  };
}

static void *prv_thread(void *arg) {
  for (;;) {
    pthread_mutex_lock(&s_lock);
    for (;;) {
      uint64_t now = posix_host_monotonic_us();
      if (s_deadline_us <= now) {
        break;
      }
      // Bounded, should the realtime clock jump.
      uint64_t wait_us = s_deadline_us - now;
      struct timespec ts = prv_realtime_in((wait_us < 100000U) ? wait_us : 100000U);
      pthread_cond_timedwait(&s_cond, &s_lock, &ts);
    }
    pthread_mutex_unlock(&s_lock);

    if (prv_take_due()) {
      pbl_posix_irq_run(prv_isr, nullptr);
    }
  }

  return nullptr;
}

static void prv_start(void) {
  pthread_t tid;
  pthread_create(&tid, nullptr, prv_thread, nullptr);
}
#endif

static int prv_cs_init(const struct pbl_device *dev) {
  pbl_clocksource_set_hz(&s_host_cs, HOST_HZ);

  return 0;
}

static int prv_ce_init(const struct pbl_device *dev) {
  pbl_clockevent_set_hz(&s_host_ce, HOST_HZ);
  prv_start();

  return 0;
}
