/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "od.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/canopen/canopen.h>
#include <pbl/canopen/cia402.h>
#include <pbl/drivers/can.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/timer.h>

#define NODE_ID 6U
#define BITRATE 500000U

/* Control loop period, and how often the main thread reports what it does */
#define LOOP_PERIOD_MS   1
#define REPORT_PERIOD_MS 10
/* Time constant of the simulated motor, and of its quick stop ramp */
#define TAU_MS    200
#define QS_TAU_MS 50
/* Target reached within this many units per second */
#define WINDOW 10
/* Fractional bits of the model's velocity */
#define Q 256

enum command {
  CMD_COAST,
  CMD_RUN,
  CMD_QUICK_STOP,
};

static struct pbl_canopen s_co;
static struct pbl_cia402 s_drive;
static struct pbl_timer s_loop;

/*
 * What the node and the control loop share. The loop never waits for the node: it only loads and
 * stores lock-free atomics, so it may as well run in a zero-latency interrupt.
 */
static atomic_int_least32_t s_target;
static atomic_int_least32_t s_velocity;
static atomic_uint_least8_t s_command;

/* The control loop, the simulated motor here, in the tick interrupt. */
static void prv_loop(struct pbl_timer *timer) {
  static int32_t velocity_q;
  enum command command = atomic_load(&s_command);
  int32_t target = command == CMD_RUN ? atomic_load(&s_target) : 0;
  int32_t tau = command == CMD_QUICK_STOP ? QS_TAU_MS : TAU_MS;
  int32_t step;

  pbl_timer_start(timer, PBL_MSEC(LOOP_PERIOD_MS));

  step = (target * Q - velocity_q) * LOOP_PERIOD_MS / tau;
  velocity_q = step == 0 ? target * Q : velocity_q + step;
  atomic_store(&s_velocity, velocity_q / Q);
}

/* Report the loop's velocity and what follows from it. */
static void prv_report(void) {
  int32_t velocity = atomic_load(&s_velocity);
  uint8_t command = atomic_load(&s_command);
  int32_t target = command == CMD_RUN ? atomic_load(&s_target) : 0;

  (void)pbl_canopen_od_write(&s_co, OD_VELOCITY_ACTUAL_VALUE, 0U, &velocity, sizeof(velocity));
  pbl_cia402_set_status(&s_drive, PBL_CIA402_SW_TARGET_REACHED,
                        abs(target - velocity) <= WINDOW ? PBL_CIA402_SW_TARGET_REACHED : 0U);

  if (command == CMD_QUICK_STOP && velocity == 0 &&
      atomic_compare_exchange_strong(&s_command, &command, CMD_COAST)) {
    pbl_cia402_quick_stop_done(&s_drive);
  }
}

static int prv_switch_on(struct pbl_cia402 *drive, void *arg) {
  printf("drive: power stage on\n");
  return 0;
}

static void prv_switch_off(struct pbl_cia402 *drive, void *arg) {
  printf("drive: power stage off\n");
}

static int prv_enable(struct pbl_cia402 *drive, void *arg) {
  int32_t target = 0;

  printf("drive: enabled\n");
  (void)pbl_canopen_od_read(&s_co, OD_TARGET_VELOCITY, 0U, &target, sizeof(target));
  atomic_store(&s_target, target);
  atomic_store(&s_command, CMD_RUN);
  return 0;
}

/* The motor coasts to a stop. */
static void prv_disable(struct pbl_cia402 *drive, void *arg) {
  printf("drive: disabled\n");
  atomic_store(&s_command, CMD_COAST);
}

/* The loop ramps the motor down, and prv_report() tells the drive once it stands still. */
static void prv_quick_stop(struct pbl_cia402 *drive, void *arg) {
  printf("drive: quick stop\n");
  atomic_store(&s_command, CMD_QUICK_STOP);
}

static int prv_set_mode(struct pbl_cia402 *drive, enum pbl_cia402_mode mode, void *arg) {
  printf("drive: mode %d\n", mode);
  return 0;
}

static const struct pbl_cia402_ops s_ops = {
  .switch_on = prv_switch_on,
  .switch_off = prv_switch_off,
  .enable = prv_enable,
  .disable = prv_disable,
  .quick_stop = prv_quick_stop,
  .set_mode = prv_set_mode,
};

/* Profile velocity: a new target goes to the loop as soon as it is written. */
static int prv_write(struct pbl_canopen *co, uint16_t index, uint8_t subindex, const void *data,
                     size_t len, void *arg) {
  if (index == OD_TARGET_VELOCITY &&
      pbl_cia402_mode(&s_drive) == PBL_CIA402_MODE_PROFILE_VELOCITY) {
    int32_t target;

    memcpy(&target, data, sizeof(target));
    atomic_store(&s_target, target);
  }

  return 0;
}

/*
 * Cyclic synchronous velocity: the target RPDO 2 applied goes to the loop, and TPDO 2 samples the
 * velocity reported here.
 */
static void prv_sync(struct pbl_canopen *co, uint8_t counter, uint32_t cycles, void *arg) {
  int32_t target;

  if (pbl_cia402_mode(&s_drive) != PBL_CIA402_MODE_CYCLIC_VELOCITY) {
    return;
  }

  if (pbl_canopen_od_read(&s_co, OD_TARGET_VELOCITY, 0U, &target, sizeof(target)) ==
      (int)sizeof(target)) {
    atomic_store(&s_target, target);
  }
  prv_report();
}

static void prv_nmt(struct pbl_canopen *co, enum pbl_canopen_nmt_state state, void *arg) {
  printf("drive: NMT %02x\n", state);
}

static const struct pbl_canopen_callbacks s_cb = {
  .nmt = prv_nmt,
  .write = prv_write,
  .sync = prv_sync,
};

int main(void) {
  const struct pbl_canopen_config cfg = {
    .can = CAN_BUS,
    .od = &od,
    .node_id = NODE_ID,
    .bitrate = BITRATE,
    .cb = &s_cb,
  };
  int ret;

  ret = pbl_canopen_init(&s_co, &cfg);
  if (ret != 0) {
    fprintf(stderr, "drive: node init failed (%d)\n", ret);
    return ret;
  }

  ret = pbl_cia402_init(&s_drive, &s_co, &s_ops, nullptr);
  if (ret != 0) {
    fprintf(stderr, "drive: drive init failed (%d)\n", ret);
    return ret;
  }

  ret = pbl_canopen_start(&s_co);
  if (ret != 0) {
    fprintf(stderr, "drive: start failed (%d)\n", ret);
    return ret;
  }

  printf("drive: node %u at %u bit/s\n", NODE_ID, BITRATE);

  pbl_timer_init(&s_loop, prv_loop);
  pbl_timer_start(&s_loop, PBL_MSEC(LOOP_PERIOD_MS));

  for (;;) {
    pbl_thread_sleep(PBL_MSEC(REPORT_PERIOD_MS));
    prv_report();
  }

  return 0;
}
