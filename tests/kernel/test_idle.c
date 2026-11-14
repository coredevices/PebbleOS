/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/idle.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <kernel.h>

#define DEEP_MIN_TICKS  10U
#define DEEP_EXIT_TICKS 2U
#define SLEEP_US        500U

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
const size_t pbl_soc_num_idle_states = ARRAY_LENGTH(pbl_soc_idle_states);

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

void test_idle__initialize(void) {
  s_entered[STATE_SHALLOW] = 0U;
  s_entered[STATE_DEEP] = 0U;
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
