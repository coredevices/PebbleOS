/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdbool.h>

struct Mutex;
struct RecursiveMutex;

typedef struct Mutex * restrict mutex_t;
typedef struct RecursiveMutex * recursive_mutex_t;

extern void mutex_lock(mutex_t);
extern void mutex_unlock(mutex_t);

extern bool mutex_lock_with_timeout(mutex_t);

extern void mutex_lock_recursive(recursive_mutex_t);
extern void mutex_unlock_recursive(recursive_mutex_t);

static mutex_t global_lock = 0;
static mutex_t global_lock2;
mutex_t recursive_lock;

void nounlock() {
  mutex_lock(global_lock);
}

void nolock() {
  mutex_unlock(global_lock);
}

void normal() {
  mutex_lock(global_lock);
  mutex_unlock(global_lock);
}

struct handle {
  mutex_t m;
} m_wrapper;

extern int do_stuff(struct handle * h);

void structthing(struct handle * h) {
  mutex_lock(h->m);
  do_stuff(h);
  mutex_unlock(h->m);
}

extern int do_stuff2();

void stuff() {
  mutex_lock(global_lock);

  do_stuff2();

  mutex_unlock(global_lock);
}

void stuff2() {
  mutex_lock(m_wrapper.m);

  do_stuff2();

  mutex_unlock(m_wrapper.m);
}

void nest2() {
  mutex_lock(global_lock);
  printf("blah %p", global_lock);
  mutex_unlock(global_lock);
}

void nest() {
  nest2();
}

void cond(void *glob_ptr) {
  mutex_lock(global_lock);

  while (glob_ptr) {
    printf("blah %p", glob_ptr);
  }

  mutex_unlock(global_lock);
}

void timeout() {
  mutex_lock_with_timeout(global_lock);

  mutex_unlock(global_lock);
}

void good_timeout() {
  if (mutex_lock_with_timeout(global_lock)) {
    mutex_unlock(global_lock);
  }
}

void stupid_timeout() {
  if (!mutex_lock_with_timeout(global_lock)) {
    mutex_unlock(global_lock);
  }
}

void reversal() {
  mutex_lock(global_lock);
  mutex_lock(global_lock2);

  mutex_unlock(global_lock);
  mutex_unlock(global_lock2);
}


// Trying to repro the false positives unsuccessfully...
extern bool decision();

inline void __attribute__((always_inline))  locker() {
  mutex_lock(global_lock);
}

inline void __attribute__((always_inline)) unlocker() {
  mutex_unlock(global_lock);
}

static inline void __attribute__((always_inline)) lock_wrap() {
  locker();
  if (decision()) {
    unlocker();
  }
}

static inline void __attribute__((always_inline)) unlock_wrap() {
  unlocker();
}

void lockme() {
  lock_wrap();
  unlock_wrap();
}
