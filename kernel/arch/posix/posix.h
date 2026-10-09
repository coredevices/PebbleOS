/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pthread.h>

struct pbl_thread;

struct posix_thread {
  pthread_t tid;
  pthread_cond_t wake;
  void (*entry)(void *);
  void *arg;
  bool created;
  bool run;
  bool aborted;
};

// Held by whoever the kernel considers to be running: a kernel thread, or a
// host thread delivering an interrupt.
extern pthread_mutex_t posix_cpu;
extern bool posix_switch_pending;
extern bool posix_in_isr;

// Hands the CPU over to the next thread, if the scheduler picks another one.
void posix_switch(void);

// Starts or resumes the given thread.
void posix_run(struct pbl_thread *t);

// Parks the calling pthread until the kernel runs it again.
void posix_park(struct posix_thread *pt);

// A pthread that will never run again; the test harness reaps them.
void posix_add_zombie(pthread_t tid);
