/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/idle.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <kernel.h>

#define DEEP_MIN_TICKS  10U
#define DEEP_EXIT_TICKS 2U
#define SLEEP_US        500U
#define HOOK_MIN_TICKS  20U
#define HOOK_EXIT_TICKS 3U
#define TICK_US         (1000000U / PBL_TICK_HZ)

enum {
  STATE_SHALLOW,
  STATE_DEEP,
};

static unsigned int s_entered[2];
static pbl_tick_t s_exit_ticks;
static uint64_t s_now_us;

static void prv_enter_shallow(void) {
  s_entered[STATE_SHALLOW]++;
  s_now_us += SLEEP_US;
}

static void prv_enter_deep(void) {
  s_entered[STATE_DEEP]++;
  s_now_us += SLEEP_US;
}

const struct pbl_idle_state pbl_soc_idle_states[] = {
  [STATE_SHALLOW] =
      {.name = "shallow", .min_ticks = 1, .exit_ticks = 0, .enter = prv_enter_shallow},
  [STATE_DEEP] = {
    .name = "deep",
    .min_ticks = DEEP_MIN_TICKS,
    .exit_ticks = DEEP_EXIT_TICKS,
    .enter = prv_enter_deep,
  },
};
const size_t pbl_soc_num_idle_states = PBL_ARRAY_SIZE(pbl_soc_idle_states);

uint64_t timekeeping_idle_begin(pbl_tick_t ticks, pbl_tick_t exit_ticks) {
  s_exit_ticks = exit_ticks;
  return s_now_us;
}

uint64_t timekeeping_idle_end(void) {
  return s_now_us;
}

unsigned int arch_irq_lock(void) {
  return 0U;
}

void arch_irq_unlock(unsigned int key) {
}

// What the hooks were called with, in order: 'A' to 'C' on enter, 'a' to 'c' on exit.
static char s_calls[32];
static size_t s_num_calls;
static pbl_idle_state_id_t s_hook_state;

static void prv_hook_enter(pbl_idle_state_id_t state, void *arg) {
  s_hook_state = state;
  if (s_num_calls < sizeof(s_calls) - 1U) {
    s_calls[s_num_calls++] = *(const char *)arg;
  }
}

static void prv_hook_exit(pbl_idle_state_id_t state, void *arg) {
  s_hook_state = state;
  if (s_num_calls < sizeof(s_calls) - 1U) {
    s_calls[s_num_calls++] = (char)(*(const char *)arg + ('a' - 'A'));
  }
}

PBL_IDLE_HOOK_DEFINE(a, prv_hook_enter, prv_hook_exit, "A");
PBL_IDLE_HOOK_DEFINE(b, prv_hook_enter, nullptr, "B");
PBL_IDLE_HOOK_DEFINE(c, nullptr, prv_hook_exit, "C");
PBL_IDLE_HOOK_DEFINE(d, prv_hook_enter, prv_hook_exit, "D");
PBL_IDLE_HOOK_LATE_DEFINE(x, prv_hook_enter, prv_hook_exit, "X");
PBL_IDLE_HOOK_LATE_DEFINE(y, prv_hook_enter, prv_hook_exit, "Y");

void test_idle__initialize(void) {
  s_entered[STATE_SHALLOW] = 0U;
  s_entered[STATE_DEEP] = 0U;
  memset(s_calls, 0, sizeof(s_calls));
  s_num_calls = 0U;

  pbl_idle_hook_set_times(PBL_IDLE_HOOK(a), 0U, 0U);
  pbl_idle_hook_set_times(PBL_IDLE_HOOK(b), 0U, 0U);
  pbl_idle_hook_set_times(PBL_IDLE_HOOK(c), HOOK_MIN_TICKS * TICK_US, HOOK_EXIT_TICKS * TICK_US);
}

void test_idle__a_long_idle_enters_the_deepest_state(void) {
  idle_enter(DEEP_MIN_TICKS);
  cl_assert_equal_i(s_entered[STATE_SHALLOW], 0);
  cl_assert_equal_i(s_entered[STATE_DEEP], 1);
  cl_assert_equal_i(s_exit_ticks, DEEP_EXIT_TICKS);
}

void test_idle__a_short_idle_enters_a_state_worth_it(void) {
  idle_enter(DEEP_MIN_TICKS - 1U);
  cl_assert_equal_i(s_entered[STATE_SHALLOW], 1);
  cl_assert_equal_i(s_entered[STATE_DEEP], 0);
}

void test_idle__no_time_to_idle_enters_no_state(void) {
  idle_enter(0U);
  cl_assert_equal_i(s_entered[STATE_SHALLOW], 0);
  cl_assert_equal_i(s_entered[STATE_DEEP], 0);
}

void test_idle__a_blocked_state_is_not_entered_until_every_block_is_released(void) {
  pbl_idle_state_block(STATE_DEEP);
  pbl_idle_state_block(STATE_DEEP);
  idle_enter(PBL_TICK_FOREVER);
  cl_assert_equal_i(s_entered[STATE_SHALLOW], 1);
  cl_assert_equal_i(s_entered[STATE_DEEP], 0);

  pbl_idle_state_release(STATE_DEEP);
  idle_enter(PBL_TICK_FOREVER);
  cl_assert_equal_i(s_entered[STATE_SHALLOW], 2);
  cl_assert_equal_i(s_entered[STATE_DEEP], 0);

  pbl_idle_state_release(STATE_DEEP);
  idle_enter(PBL_TICK_FOREVER);
  cl_assert_equal_i(s_entered[STATE_SHALLOW], 2);
  cl_assert_equal_i(s_entered[STATE_DEEP], 1);
}

void test_idle__blocking_a_state_blocks_the_deeper_ones(void) {
  pbl_idle_state_block(STATE_SHALLOW);
  idle_enter(PBL_TICK_FOREVER);
  pbl_idle_state_release(STATE_SHALLOW);
  cl_assert_equal_i(s_entered[STATE_SHALLOW], 0);
  cl_assert_equal_i(s_entered[STATE_DEEP], 0);
}

void test_idle__disabled_enters_no_state(void) {
  pbl_idle_set_enabled(false);
  idle_enter(PBL_TICK_FOREVER);
  pbl_idle_set_enabled(true);
  cl_assert_equal_i(s_entered[STATE_SHALLOW], 0);
  cl_assert_equal_i(s_entered[STATE_DEEP], 0);
}

void test_idle__the_time_in_a_state_adds_to_its_residency(void) {
  uint64_t before_us = pbl_idle_residency_us(STATE_DEEP);

  idle_enter(PBL_TICK_FOREVER);
  idle_enter(PBL_TICK_FOREVER);
  cl_assert_equal_i(pbl_idle_residency_us(STATE_DEEP) - before_us, 2U * SLEEP_US);
}

void test_idle__hooks_exit_in_the_opposite_order_they_enter(void) {
  idle_enter(PBL_TICK_FOREVER);
  cl_assert_equal_s(s_calls, "ABca");
}

void test_idle__hooks_are_told_the_state(void) {
  idle_enter(DEEP_MIN_TICKS - 1U);
  cl_assert_equal_i(s_hook_state, STATE_SHALLOW);

  idle_enter(DEEP_MIN_TICKS);
  cl_assert_equal_i(s_hook_state, STATE_DEEP);
}

void test_idle__a_hook_is_not_called_for_an_idle_shorter_than_its_minimum(void) {
  idle_enter(HOOK_MIN_TICKS - 1U);
  cl_assert_equal_s(s_calls, "ABa");
  cl_assert_equal_i(s_exit_ticks, DEEP_EXIT_TICKS);
}

void test_idle__the_cpu_wakes_up_earlier_for_the_hooks(void) {
  idle_enter(HOOK_MIN_TICKS);
  cl_assert_equal_s(s_calls, "ABca");
  cl_assert_equal_i(s_exit_ticks, DEEP_EXIT_TICKS + HOOK_EXIT_TICKS);
}

void test_idle__a_hook_is_not_called_until_its_times_are_set(void) {
  idle_enter(PBL_TICK_FOREVER);
  cl_assert(strchr(s_calls, 'D') == nullptr);
  cl_assert(strchr(s_calls, 'd') == nullptr);
}

void test_idle__late_hooks_are_called_apart(void) {
  pbl_idle_hooks_late_enter(STATE_DEEP);
  pbl_idle_hooks_late_exit(STATE_DEEP);
  cl_assert_equal_s(s_calls, "XYyx");
}

void test_idle__entering_a_state_calls_no_late_hook(void) {
  idle_enter(PBL_TICK_FOREVER);
  cl_assert(strchr(s_calls, 'X') == nullptr);
}

void test_idle__a_latency_request_keeps_out_of_the_states_slower_to_wake_up(void) {
  struct pbl_idle_latency_request slow;
  struct pbl_idle_latency_request fast;

  pbl_idle_latency_request_add(&slow, (DEEP_EXIT_TICKS + 1U) * TICK_US);
  idle_enter(PBL_TICK_FOREVER);
  cl_assert_equal_i(s_entered[STATE_DEEP], 1);

  // The shortest request applies.
  pbl_idle_latency_request_add(&fast, (DEEP_EXIT_TICKS - 1U) * TICK_US);
  idle_enter(PBL_TICK_FOREVER);
  cl_assert_equal_i(s_entered[STATE_SHALLOW], 1);
  cl_assert_equal_i(s_entered[STATE_DEEP], 1);

  pbl_idle_latency_request_remove(&fast);
  idle_enter(PBL_TICK_FOREVER);
  cl_assert_equal_i(s_entered[STATE_DEEP], 2);

  pbl_idle_latency_request_remove(&slow);
}

void test_idle__a_latency_request_keeps_the_hooks_slower_to_wake_up_from_being_called(void) {
  struct pbl_idle_latency_request req;

  pbl_idle_latency_request_add(&req, (DEEP_EXIT_TICKS + HOOK_EXIT_TICKS - 1U) * TICK_US);
  idle_enter(PBL_TICK_FOREVER);
  pbl_idle_latency_request_remove(&req);
  cl_assert_equal_i(s_entered[STATE_DEEP], 1);
  cl_assert_equal_s(s_calls, "ABa");
}
