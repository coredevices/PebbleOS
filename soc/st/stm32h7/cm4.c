/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "cm4.h"

#include <pbl/devicetree.h>
#include <pbl/drivers/mbox.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/work.h>
#include <pbl/util/misc.h>

// The Cortex-M7 tells this core before powering the system off, whose Standby needs this core
// stopped too, or to stop it: it powers off, its drivers first. This core asks it for the clocks
// when only this core woke from Stop.
struct pbl_stm32h7_cm4 {
  struct pbl_device dev;
  struct pbl_mbox_client cl;
  struct pbl_mbox_chan shutdown;
  struct pbl_mbox_client clocks_cl;
  struct pbl_mbox_chan clocks;
};

static const struct pbl_stm32h7_cm4 *s_cm4;

void pbl_soc_stm32h7_cm4_clocks_request(void) {
  if (s_cm4 != nullptr) {
    (void)pbl_mbox_send(&s_cm4->clocks, nullptr);
  }
}

static void prv_power_off(struct pbl_work *work) {
  pbl_power_off();
}

static PBL_WORK_DEFINE(s_power_off, prv_power_off);

static void prv_rx(const struct pbl_mbox_client *cl, const void *msg) {
  (void)pbl_work_submit(&pbl_sys_workq, &s_power_off);
}

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_stm32h7_cm4 *cm4 = PBL_CONTAINER_OF(dev, const struct pbl_stm32h7_cm4, dev);

  int ret;

  ret = pbl_mbox_request(&cm4->shutdown, &cm4->cl);
  if (ret == 0 && cm4->clocks.ctrl != nullptr) {
    ret = pbl_mbox_request(&cm4->clocks, &cm4->clocks_cl);
    if (ret == 0) {
      s_cm4 = cm4;
    }
  }

  return ret;
}

#define PBL_STM32H7_CM4_DEFINE(sym, _name, _shutdown, _clocks)                            \
  PBL_DEVICE_STATE_DEFINE(sym);                                                           \
  PBL_DEVICE_INSTANCE_DEV(sym, struct pbl_stm32h7_cm4) {                                  \
    .dev = PBL_DEVICE_INIT(sym, _name, prv_init, nullptr), .cl = {.rx_callback = prv_rx}, \
    .shutdown = _shutdown, .clocks = _clocks,                                             \
  }

#include PBL_DT_INSTANCES(st_stm32h747_m4_device)
