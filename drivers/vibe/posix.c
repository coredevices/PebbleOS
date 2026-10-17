/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/vibe.h>
#include <pbl/drivers/vibe/posix.h>

static int prv_init(const struct pbl_vibe *vibe) {
  return 0;
}

static int prv_start(const struct pbl_vibe *vibe, int8_t strength) {
  return 0;
}

static int prv_stop(const struct pbl_vibe *vibe) {
  return 0;
}

static const struct pbl_vibe_ops s_ops = {
  .init = prv_init,
  .start = prv_start,
  .stop = prv_stop,
};

PBL_VIBE_STATE_DEFINE(pbl_vibe_posix);
const struct pbl_vibe pbl_vibe_posix =
    PBL_VIBE_INIT(pbl_vibe_posix, "vibe", nullptr, &s_ops, VIBE_STRENGTH_OFF);
PBL_DEVICE_REGISTER(pbl_vibe_posix, &pbl_vibe_posix.dev);
