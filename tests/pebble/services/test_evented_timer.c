/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/evented_timer.h>

#include <clar.h>
#include <fake_pbl_malloc.h>
#include <fake_work.h>
#include <kernel/events.h>
#include <kernel/pebble_tasks.h>

// Fakes
///////////////////////////////////////////////////////////
static PebbleTask s_current_task;

PebbleTask pebble_task_get_current(void) {
  return s_current_task;
}

const char *pebble_task_get_name(PebbleTask task) {
  return "App <Stub>";
}

static PebbleEvent s_last_event;
static EventedTimerID s_cancel_while_posting;

bool process_manager_send_event_to_process(PebbleTask task, PebbleEvent *e) {
  s_last_event = *e;
  if (s_cancel_while_posting != EVENTED_TIMER_INVALID_ID) {
    evented_timer_cancel(s_cancel_while_posting);
  }
  return true;
}

// The queue, for what the service submits directly.
#define MAX_SUBMITTED 256

static struct pbl_work *s_submitted[MAX_SUBMITTED];
static size_t s_num_submitted;

bool pbl_work_submit(struct pbl_workq *q, struct pbl_work *work) {
  cl_assert(q == &pbl_sys_workq_high);
  cl_assert(s_num_submitted < MAX_SUBMITTED);
  s_submitted[s_num_submitted++] = work;
  return true;
}

static void prv_run_submitted(void) {
  for (size_t i = 0; i < s_num_submitted; i++) {
    s_submitted[i]->fn(s_submitted[i]);
  }
  s_num_submitted = 0;
}

// Tests
///////////////////////////////////////////////////////////
int s_times_callback_executed = 0;

static void stub_evented_timer_callback(void *data) {
  s_times_callback_executed++;
  s_last_event = (PebbleEvent){};
}

static void prv_run_event(void) {
  s_last_event.callback.callback(s_last_event.callback.data);
}

void test_evented_timer__initialize(void) {
  s_current_task = PebbleTask_App;
  s_times_callback_executed = 0;
  s_cancel_while_posting = EVENTED_TIMER_INVALID_ID;
  s_num_submitted = 0;

  s_last_event = (PebbleEvent){};
}

void test_evented_timer__cleanup(void) {
  s_cancel_while_posting = EVENTED_TIMER_INVALID_ID;
  evented_timer_clear_process_timers(PebbleTask_App);
  evented_timer_clear_process_timers(PebbleTask_Worker);
  prv_run_submitted();
  fake_pbl_malloc_check_net_allocs();
  evented_timer_reset();
  fake_work_cleanup();
  fake_pbl_malloc_clear_tracking();
}

void test_evented_timer__simple(void) {
  EventedTimerID e_timer = evented_timer_register(100, false, stub_evented_timer_callback, 0);

  cl_assert(e_timer != EVENTED_TIMER_INVALID_ID);
  struct pbl_delayed_work *dwork = fake_work_get_next();
  cl_assert(fake_work_is_scheduled(dwork));
  cl_assert_equal_i(s_last_event.type, PEBBLE_NULL_EVENT);
  cl_assert(!s_times_callback_executed);

  // Fire the timer on "the timer task"
  fake_work_fire(dwork);

  // We know have an event to run on "the app task"
  cl_assert_equal_i(s_last_event.type, PEBBLE_CALLBACK_EVENT);
  cl_assert(!s_times_callback_executed);

  // Run the code on "the app task"
  prv_run_event();

  // And we're done!
  cl_assert(s_times_callback_executed == 1);

  // Nothing is left to fire (not a repeating timer)
  cl_assert(fake_work_get_next() == nullptr);
  cl_assert(!evented_timer_exists(e_timer));
  cl_assert_equal_i(s_last_event.type, PEBBLE_NULL_EVENT);
}

void test_evented_timer__repeating(void) {
  EventedTimerID e_timer = evented_timer_register(100, true, stub_evented_timer_callback, 0);

  struct pbl_delayed_work *dwork = fake_work_get_next();
  cl_assert(fake_work_is_scheduled(dwork));
  cl_assert_equal_i(s_last_event.type, PEBBLE_NULL_EVENT);
  cl_assert(!s_times_callback_executed);

  for (int i = 0; i < 10; ++i) {
    // Fire the timer on "the timer task"
    fake_work_fire(dwork);

    // It is armed again for the same period
    cl_assert(fake_work_is_scheduled(dwork));
    cl_assert_equal_i(fake_work_timeout_ms(dwork), 100);

    // We know have an event to run on "the app task"
    cl_assert_equal_i(s_last_event.type, PEBBLE_CALLBACK_EVENT);
    cl_assert(s_times_callback_executed == i);

    // Run the code on "the app task"
    prv_run_event();

    // And we're done!
    cl_assert(s_times_callback_executed == i + 1);
    cl_assert(evented_timer_exists(e_timer));
  }
}

void test_evented_timer__repeating_keeps_rescheduled_period(void) {
  EventedTimerID e_timer = evented_timer_register(100, true, stub_evented_timer_callback, 0);
  struct pbl_delayed_work *dwork = fake_work_get_next();

  cl_assert(evented_timer_reschedule(e_timer, 300));
  cl_assert_equal_i(fake_work_timeout_ms(dwork), 300);

  fake_work_fire(dwork);
  cl_assert_equal_i(fake_work_timeout_ms(dwork), 300);
}

void test_evented_timer__repeating_rescheduled_while_running(void) {
  EventedTimerID e_timer = evented_timer_register(100, true, stub_evented_timer_callback, 0);
  struct pbl_delayed_work *dwork = fake_work_get_next();

  // The run is dequeued, then the reschedule gets the mutex first: the new delay stands.
  fake_work_set_running(dwork);
  cl_assert(evented_timer_reschedule(e_timer, 300));
  dwork->work.fn(&dwork->work);

  cl_assert_equal_i(s_last_event.type, PEBBLE_NULL_EVENT);
  cl_assert(fake_work_is_scheduled(dwork));
  cl_assert_equal_i(fake_work_timeout_ms(dwork), 300);
}

void test_evented_timer__cancel_stops_repeating_while_posting(void) {
  EventedTimerID e_timer = evented_timer_register(100, true, stub_evented_timer_callback, 0);
  struct pbl_delayed_work *dwork = fake_work_get_next();

  s_cancel_while_posting = e_timer;
  fake_work_fire(dwork);

  // The run must not arm the timer again, and it is still allocated until this point.
  cl_assert(fake_work_get_next() == nullptr);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 1);
  prv_run_submitted();
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);

  // The event it posted finds no timer.
  prv_run_event();
  cl_assert(!s_times_callback_executed);
}

void test_evented_timer__reschedule(void) {
  EventedTimerID timer = evented_timer_register(100, false, stub_evented_timer_callback, 0);
  struct pbl_delayed_work *dwork = fake_work_get_next();

  cl_assert(evented_timer_reschedule(timer, 200));
  cl_assert_equal_i(fake_work_timeout_ms(dwork), 200);

  // Too late once it fired, even if the event did not reach the app yet.
  fake_work_fire(dwork);
  cl_assert(!evented_timer_reschedule(timer, 200));
  cl_assert(!fake_work_is_scheduled(dwork));

  prv_run_event();
  cl_assert_equal_i(s_times_callback_executed, 1);
  cl_assert(!evented_timer_reschedule(timer, 200));
}

void test_evented_timer__reschedule_one_shot_while_running(void) {
  EventedTimerID timer = evented_timer_register(100, false, stub_evented_timer_callback, 0);
  struct pbl_delayed_work *dwork = fake_work_get_next();

  fake_work_set_running(dwork);
  cl_assert(!evented_timer_reschedule(timer, 200));
  fake_work_cleanup();
}

void test_evented_timer__huge_timeout_is_not_forever(void) {
  evented_timer_register(UINT32_MAX, false, stub_evented_timer_callback, 0);

  const uint64_t expected = ((uint64_t)UINT32_MAX * PBL_TICK_HZ) / 1000U;
  cl_assert_equal_i(fake_work_get_next()->timer.expires,
                    expected < PBL_TICK_FOREVER ? expected : PBL_TICK_FOREVER - 1U);
}

void test_evented_timer__cancel_during_work(void) {
  EventedTimerID timer = evented_timer_register(100, false, stub_evented_timer_callback, 0);

  // We've started the timer, but it hasn't fired yet.
  struct pbl_delayed_work *dwork = fake_work_get_next();
  cl_assert(fake_work_is_scheduled(dwork));
  cl_assert_equal_i(s_last_event.type, PEBBLE_NULL_EVENT);
  cl_assert(!s_times_callback_executed);

  // A run was dequeued and waits for the mutex, which the cancel has. The cancel must not wait for
  // the run, and must leave it something valid to read.
  pbl_work_fn_t stale_run = dwork->work.fn;
  fake_work_set_running(dwork);
  evented_timer_cancel(timer);
  cl_assert(!evented_timer_exists(timer));
  cl_assert(!fake_work_is_scheduled(dwork));
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 1);

  // While that memory is still taken, a new timer cannot have the address of the old one.
  EventedTimerID new_timer = evented_timer_register(100, false, stub_evented_timer_callback, 0);
  cl_assert(new_timer != timer);
  struct pbl_delayed_work *new_dwork = fake_work_get_next();
  cl_assert(new_dwork != dwork);

  // The stale run generates no event and leaves the new timer alone.
  stale_run(&dwork->work);
  cl_assert_equal_i(s_last_event.type, PEBBLE_NULL_EVENT);
  cl_assert(!s_times_callback_executed);
  cl_assert(fake_work_is_scheduled(new_dwork));

  // The queue gets to the old timer after the run, and that frees it.
  prv_run_submitted();
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 1);
  fake_work_cleanup();
}

void test_evented_timer__cancel_during_app_event(void) {
  EventedTimerID timer = evented_timer_register(100, false, stub_evented_timer_callback, 0);

  // We've started the timer, but it hasn't fired yet.
  struct pbl_delayed_work *dwork = fake_work_get_next();
  cl_assert(fake_work_is_scheduled(dwork));
  cl_assert_equal_i(s_last_event.type, PEBBLE_NULL_EVENT);
  cl_assert(!s_times_callback_executed);

  // Fire the timer on "the timer task"
  fake_work_fire(dwork);

  // We know have an event to run on "the app task"
  cl_assert_equal_i(s_last_event.type, PEBBLE_CALLBACK_EVENT);
  cl_assert(!s_times_callback_executed);

  // Now cancel the timer after the event has been run on the timer task but before it's handled on
  // the app task
  evented_timer_cancel(timer);

  // Run the code on "the app task"
  prv_run_event();

  // And we're done! Even though we let the timer fire and generate the event, cancelling before the
  // event is handled should stop the registered timer callback from being called.
  cl_assert(!s_times_callback_executed);
}

void test_evented_timer__stale_id_never_matches(void) {
  EventedTimerID old_timer = evented_timer_register(100, false, stub_evented_timer_callback, 0);
  evented_timer_cancel(old_timer);
  prv_run_submitted();

  // Even at the same address, as the fake's heap may hand out the same block.
  for (int i = 0; i < 100; i++) {
    EventedTimerID timer = evented_timer_register(100, false, stub_evented_timer_callback, 0);
    cl_assert(timer != EVENTED_TIMER_INVALID_ID);
    cl_assert(timer != old_timer);
    cl_assert(!evented_timer_exists(old_timer));
    evented_timer_cancel(old_timer);
    cl_assert(evented_timer_exists(timer));
    evented_timer_cancel(timer);
    prv_run_submitted();
    old_timer = timer;
  }
}

void test_evented_timer__process_out_of_memory_gets_an_error(void) {
  s_current_task = PebbleTask_App;
  fake_malloc_set_largest_free_block(1);
  cl_assert_equal_i(evented_timer_register(100, false, stub_evented_timer_callback, 0),
                    EVENTED_TIMER_INVALID_ID);
  fake_malloc_set_largest_free_block(~0);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
  cl_assert(fake_work_get_next() == nullptr);
}

void test_evented_timer__max_per_process(void) {
  // Fired timers are still counted until their event is consumed.
  for (int i = 0; i < CONFIG_SERVICE_EVENTED_TIMER_MAX_PER_PROCESS; i++) {
    cl_assert(evented_timer_register(100, false, stub_evented_timer_callback, 0) !=
              EVENTED_TIMER_INVALID_ID);
    fake_work_fire(fake_work_get_next());
  }
  cl_assert_equal_i(evented_timer_register(100, false, stub_evented_timer_callback, 0),
                    EVENTED_TIMER_INVALID_ID);

  // Another process has its own count.
  s_current_task = PebbleTask_Worker;
  cl_assert(evented_timer_register(100, false, stub_evented_timer_callback, 0) !=
            EVENTED_TIMER_INVALID_ID);
  fake_work_cleanup();
}

void test_evented_timer__clear_process_timers(void) {
  EventedTimerID app_timers[20];
  EventedTimerID worker_timers[5];

  for (size_t i = 0; i < ARRAY_LENGTH(app_timers); i++) {
    app_timers[i] = evented_timer_register(100, i % 2, stub_evented_timer_callback, 0);
  }
  s_current_task = PebbleTask_Worker;
  for (size_t i = 0; i < ARRAY_LENGTH(worker_timers); i++) {
    worker_timers[i] = evented_timer_register(50, false, stub_evented_timer_callback, 0);
  }

  evented_timer_clear_process_timers(PebbleTask_App);

  for (size_t i = 0; i < ARRAY_LENGTH(app_timers); i++) {
    cl_assert(!evented_timer_exists(app_timers[i]));
  }
  for (size_t i = 0; i < ARRAY_LENGTH(worker_timers); i++) {
    cl_assert(evented_timer_exists(worker_timers[i]));
  }
  // Nothing ran, so nothing had to wait.
  cl_assert_equal_i(s_num_submitted, 0);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), ARRAY_LENGTH(worker_timers));

  // Only the worker's are still armed.
  for (size_t i = 0; i < ARRAY_LENGTH(worker_timers); i++) {
    fake_work_fire(fake_work_get_next());
  }
  cl_assert(fake_work_get_next() == nullptr);
}
