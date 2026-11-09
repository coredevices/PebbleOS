/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/thread.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>

/* A work item lives in the object it acts on, which its function gets back. */
struct sensor {
  const char *name;
  unsigned int pending_events;
  unsigned int runs;
  struct pbl_work work;
};

static void prv_sensor_work(struct pbl_work *work) {
  struct sensor *sensor = container_of(work, struct sensor, work);

  /* One run serves every event since the last one, however often it was submitted. */
  PBL_LOG_INFO("%s: handling %u events on %s", sensor->name, sensor->pending_events,
               pbl_thread_name(pbl_thread_current()));
  sensor->pending_events = 0;
  sensor->runs++;
}

static void prv_sensor_event(struct pbl_workq *q, struct sensor *sensor) {
  sensor->pending_events++;
  if (!pbl_work_submit(q, &sensor->work)) {
    PBL_LOG_INFO("%s: already pending, not queued again", sensor->name);
  }
}

static struct sensor s_accel = {.name = "accel", .work = PBL_WORK_INITIALIZER(prv_sensor_work)};
static struct sensor s_touch = {.name = "touch", .work = PBL_WORK_INITIALIZER(prv_sensor_work)};

/* A queue of its own keeps slow work from delaying the system's. */
static PBL_WORKQ_DEFINE(s_sensor_q);
PBL_THREAD_STACK_DEFINE(s_sensor_q_stack, 1024);
static struct pbl_thread s_sensor_q_thread;

int main(void) {
  const struct pbl_thread_attr attr = {
    .name = "sensor_q",
    .entry = pbl_workq_thread,
    .arg = &s_sensor_q,
    .prio = CONFIG_KERNEL_MAIN_THREAD_PRIO - 1,
    .privileged = true,
    .stack = s_sensor_q_stack,
    .stack_size = sizeof(s_sensor_q_stack),
  };

  /* Both queues run below main, so nothing runs until main waits for them. */
  PBL_LOG_INFO("main: three accel events for the system queue");
  for (unsigned int i = 0; i < 3; i++) {
    prv_sensor_event(&pbl_sys_workq, &s_accel);
  }
  pbl_workq_flush(&pbl_sys_workq);
  PBL_LOG_INFO("main: accel work ran %u time(s)", s_accel.runs);

  PBL_LOG_INFO("main: touch and accel events for a queue of our own");
  pbl_thread_create(&s_sensor_q_thread, &attr);
  prv_sensor_event(&s_sensor_q, &s_touch);
  prv_sensor_event(&s_sensor_q, &s_accel);
  pbl_workq_flush(&s_sensor_q);

  PBL_LOG_INFO("main: a touch event, canceled before the queue gets to it");
  prv_sensor_event(&s_sensor_q, &s_touch);
  if (pbl_work_cancel_sync(&s_touch.work)) {
    PBL_LOG_INFO("main: touch work was pending and will not run");
  }
  pbl_workq_flush(&s_sensor_q);
  PBL_LOG_INFO("main: touch work ran %u time(s)", s_touch.runs);

  PBL_LOG_INFO("Sample done");

  return 0;
}
