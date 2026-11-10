/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl_arch_thread.h>

/** @cond INTERNAL_HIDDEN */

struct pbl_thread;
struct pbl_msgq;
struct pbl_poll_group;

/* Threads blocked on an object, highest priority first, FIFO within a priority. */
struct pbl_waitq {
  struct pbl_thread *head;
};

/* Entry of the timeout queue: a blocked thread, or a timer when @c thread is NULL. */
struct pbl_timeout {
  struct pbl_timeout *next;
  uint64_t expires;
  struct pbl_thread *thread;
  bool active;
};

struct pbl_thread_backend {
  void *sp; // saved stack pointer; must stay first
  struct pbl_arch_thread arch;
  struct pbl_thread *next;     // ready list or wait queue link
  struct pbl_thread *all_next; // list of every live thread
  struct pbl_waitq *waitq;     // object blocked on, NULL for a plain sleep
  struct pbl_timeout timeout;
  uint32_t run_time;
  uint32_t number;
  uint8_t state;
  uint8_t base_prio;
  uint8_t mutexes_held;
  int wake_rc;
};

struct pbl_mutex_backend {
  struct pbl_waitq waitq;
};

struct pbl_sem_backend {
  struct pbl_waitq waitq;
  uint32_t count;
};

struct pbl_msgq_backend {
  struct pbl_waitq getters;
  struct pbl_waitq putters;
  uint32_t head;
  uint32_t tail;
  uint32_t count;
};

struct pbl_poll_group_backend {
  struct pbl_waitq waitq;
  struct pbl_msgq *cursor;
};

#define PBL_SEM_BACKEND_INITIALIZER(initial) {.count = (initial)}

/** @endcond */
