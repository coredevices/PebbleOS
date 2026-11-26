/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/can.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/msgq.h>
#include <pbl/kernel/time.h>

#define BITRATE   500000U
#define PING_ID   0x100U
#define ECHO_ID   0x101U
#define PINGS     10U
#define ECHO_MS   100U
#define PERIOD_MS 100U

static PBL_MSGQ_DEFINE(s_rx_q, sizeof(struct pbl_can_frame), 4);

/* Called from the controller interrupt: only queue the frame. */
static void prv_rx(const struct pbl_can *can, const struct pbl_can_frame *frame, void *arg) {
  (void)pbl_msgq_put(&s_rx_q, frame, PBL_NO_WAIT);
}

/* Wait for the echo of ping @p seq, skipping other frames. */
static int prv_wait_echo(uint32_t seq) {
  uint64_t end = pbl_uptime_ms() + ECHO_MS;
  struct pbl_can_frame frame;

  for (uint64_t now = pbl_uptime_ms(); now < end; now = pbl_uptime_ms()) {
    if (pbl_msgq_get(&s_rx_q, &frame, PBL_MSEC((uint32_t)(end - now))) != 0) {
      break;
    }
    if (frame.id == ECHO_ID && frame.len == 4U && frame.data32[0] == seq) {
      return 0;
    }
  }

  return -ETIMEDOUT;
}

int main(void) {
  const struct pbl_can_config cfg = {.bitrate = BITRATE};
  const struct pbl_can_filter filter = {.id = ECHO_ID, .mask = PBL_CAN_STD_ID_MASK};
  struct pbl_can_frame ping = {.id = PING_ID, .len = 4U};
  struct pbl_can_err_cnt cnt;
  enum pbl_can_bus_state state;
  unsigned int echoed = 0U;
  int ret;

  ret = pbl_can_configure(CAN_BUS, &cfg);
  if (ret >= 0) {
    ret = pbl_can_add_filter(CAN_BUS, &filter);
  }
  if (ret < 0) {
    fprintf(stderr, "can: configuration failed (%d)\n", ret);
    return ret;
  }

  pbl_can_set_rx_callback(CAN_BUS, prv_rx, nullptr);

  ret = pbl_can_start(CAN_BUS);
  if (ret != 0) {
    fprintf(stderr, "can: start failed (%d)\n", ret);
    return ret;
  }

  printf("can: %u bit/s\n", BITRATE);

  for (uint32_t seq = 0U; seq < PINGS; seq++) {
    uint64_t start = pbl_uptime_ms();

    /* Without a callback, wait for the frame to go: another node has to acknowledge it. */
    ping.data32[0] = seq;
    ret = pbl_can_send(CAN_BUS, &ping, PBL_MSEC(ECHO_MS), nullptr, nullptr);
    if (ret != 0) {
      fprintf(stderr, "can: ping %u not sent (%d), no other node on the bus?\n", (unsigned int)seq,
              ret);
      break;
    }

    if (prv_wait_echo(seq) == 0) {
      printf("can: echo %u in %u ms\n", (unsigned int)seq, (unsigned int)(pbl_uptime_ms() - start));
      echoed++;
    } else {
      printf("can: no echo %u\n", (unsigned int)seq);
    }

    pbl_sleep_ms(PERIOD_MS);
  }

  (void)pbl_can_get_state(CAN_BUS, &state, &cnt);
  printf("can: %u of %u echoed, tx errors %u, rx errors %u\n", echoed, PINGS, cnt.tx, cnt.rx);

  (void)pbl_can_stop(CAN_BUS);

  printf("Sample done\n");

  return 0;
}
