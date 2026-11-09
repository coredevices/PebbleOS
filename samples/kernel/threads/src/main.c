/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/thread.h>
#include <pbl/logging/logging.h>

#define ROUNDS 3

/* main() runs at CONFIG_KERNEL_MAIN_THREAD_PRIO: one thread above it, two below. */
#define URGENT_PRIO (CONFIG_KERNEL_MAIN_THREAD_PRIO + 1)
#define PEER_PRIO   (CONFIG_KERNEL_MAIN_THREAD_PRIO - 1)

PBL_THREAD_STACK_DEFINE(s_urgent_stack, 1024);
PBL_THREAD_STACK_DEFINE(s_ping_stack, 1024);
PBL_THREAD_STACK_DEFINE(s_pong_stack, 1024);

static struct pbl_thread s_urgent;
static struct pbl_thread s_ping;
static struct pbl_thread s_pong;

static void prv_urgent(void *arg) {
  PBL_LOG_INFO("urgent: preempted main, which is still inside pbl_thread_create()");
}

static void prv_peer(void *arg) {
  unsigned int *rounds = arg;
  const char *name = pbl_thread_name(pbl_thread_current());

  for (*rounds = 0; *rounds < ROUNDS; (*rounds)++) {
    PBL_LOG_INFO("%s: round %u", name, *rounds);
    /* Threads of equal priority also take turns on every tick; yielding does it now. */
    pbl_thread_yield();
  }
}

static void prv_start(struct pbl_thread *t, const char *name, pbl_thread_entry_t entry, void *arg,
                      pbl_prio_t prio, void *stack, size_t stack_size) {
  const struct pbl_thread_attr attr = {
    .name = name,
    .entry = entry,
    .arg = arg,
    .prio = prio,
    .privileged = true,
    .stack = stack,
    .stack_size = stack_size,
  };

  pbl_thread_create(t, &attr);
}

int main(void) {
  static unsigned int s_ping_rounds;
  static unsigned int s_pong_rounds;

  PBL_LOG_INFO("main: creating a thread of higher priority");
  prv_start(&s_urgent, "urgent", prv_urgent, NULL, URGENT_PRIO, s_urgent_stack,
            sizeof(s_urgent_stack));
  PBL_LOG_INFO("main: urgent has %s",
               pbl_thread_state(&s_urgent) == PBL_THREAD_DEAD ? "ended" : "not ended");

  prv_start(&s_ping, "ping", prv_peer, &s_ping_rounds, PEER_PRIO, s_ping_stack,
            sizeof(s_ping_stack));
  prv_start(&s_pong, "pong", prv_peer, &s_pong_rounds, PEER_PRIO, s_pong_stack,
            sizeof(s_pong_stack));
  PBL_LOG_INFO("main: created two threads of lower priority, which wait for main to block");

  while (pbl_thread_state(&s_ping) != PBL_THREAD_DEAD ||
         pbl_thread_state(&s_pong) != PBL_THREAD_DEAD) {
    pbl_thread_sleep(PBL_MSEC(10));
  }

  PBL_LOG_INFO("main: ping ran %u rounds, pong ran %u rounds", s_ping_rounds, s_pong_rounds);
  PBL_LOG_INFO("Sample done");

  return 0;
}
