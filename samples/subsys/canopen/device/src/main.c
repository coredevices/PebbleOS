/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "od.h"

#include <stdint.h>
#include <stdio.h>

#include <pbl/canopen/canopen.h>
#include <pbl/drivers/can.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>

#define NODE_ID   5U
#define BITRATE   500000U
#define PERIOD_MS 500U
#define REPORT_MS 5000U

static struct pbl_canopen s_co;
static uint32_t s_syncs;
static uint32_t s_sync_max_us;

static const char *prv_state_name(enum pbl_canopen_nmt_state state) {
  switch (state) {
    case PBL_CANOPEN_NMT_INITIALIZING:
      return "initializing";
    case PBL_CANOPEN_NMT_STOPPED:
      return "stopped";
    case PBL_CANOPEN_NMT_OPERATIONAL:
      return "operational";
    case PBL_CANOPEN_NMT_PRE_OPERATIONAL:
      return "pre-operational";
    default:
      return "?";
  }
}

static void prv_nmt(struct pbl_canopen *co, enum pbl_canopen_nmt_state state, void *arg) {
  printf("canopen: NMT %s\n", prv_state_name(state));
}

static void prv_reset(struct pbl_canopen *co, enum pbl_canopen_reset reset, void *arg) {
  printf("canopen: reset %s\n", reset == PBL_CANOPEN_RESET_NODE ? "node" : "communication");
}

/* Every write from the bus, by an SDO or the RPDO, goes through here first. */
static int prv_write(struct pbl_canopen *co, uint16_t index, uint8_t subindex, const void *data,
                     size_t len, void *arg) {
  if (index == OD_OUTPUT) {
    printf("canopen: output %u\n", *(const uint8_t *)data);
  } else if (index == OD_LABEL) {
    printf("canopen: label %.*s\n", (int)len, (const char *)data);
  }

  return 0;
}

/* A control loop would run its step here: how late it runs after the SYNC came in. */
static void prv_sync(struct pbl_canopen *co, uint8_t counter, uint32_t cycles, void *arg) {
  uint32_t us =
      (uint32_t)((uint64_t)(pbl_cycle_get_32() - cycles) * 1000000U / pbl_cycles_per_sec());

  s_syncs++;
  if (us > s_sync_max_us) {
    s_sync_max_us = us;
  }
}

static const struct pbl_canopen_callbacks s_cb = {
  .nmt = prv_nmt,
  .reset = prv_reset,
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
    fprintf(stderr, "canopen: init failed (%d)\n", ret);
    return ret;
  }

  ret = pbl_canopen_start(&s_co);
  if (ret != 0) {
    fprintf(stderr, "canopen: start failed (%d)\n", ret);
    return ret;
  }

  printf("canopen: node %u at %u bit/s\n", NODE_ID, BITRATE);

  /* The counter goes out in TPDO 1 on every change, while operational. */
  for (uint32_t counter = 1U;; counter++) {
    pbl_thread_sleep(PBL_MSEC(PERIOD_MS));
    (void)pbl_canopen_od_write(&s_co, OD_COUNTER, 0U, &counter, sizeof(counter));

    if ((counter % (REPORT_MS / PERIOD_MS)) == 0U && s_syncs != 0U) {
      printf("canopen: %u SYNC, latency up to %u us\n", (unsigned int)s_syncs,
             (unsigned int)s_sync_max_us);
      s_syncs = 0U;
      s_sync_max_us = 0U;
    }
  }

  return 0;
}
