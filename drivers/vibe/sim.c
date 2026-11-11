/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/vibe.h>
#include <pbl/drivers/vibe/sim.h>

static int8_t s_strength;
static pbl_vibe_sim_observer_t s_observer;
static void *s_observer_arg;

void pbl_vibe_sim_set_observer(const struct pbl_vibe *vibe, pbl_vibe_sim_observer_t observer,
                               void *arg) {
  pbl_mutex_lock(&vibe->state->lock, PBL_FOREVER);
  s_observer = observer;
  s_observer_arg = arg;
  pbl_mutex_unlock(&vibe->state->lock);
}

int8_t pbl_vibe_sim_get_strength(const struct pbl_vibe *vibe) {
  return s_strength;
}

static int prv_set(const struct pbl_vibe *vibe, int8_t strength) {
  if (s_strength == strength) {
    return 0;
  }

  s_strength = strength;

  if (s_observer != nullptr) {
    s_observer(vibe, strength, s_observer_arg);
  }

  return 0;
}

static int prv_init(const struct pbl_vibe *vibe) {
  s_strength = 0;

  return 0;
}

static int prv_start(const struct pbl_vibe *vibe, int8_t strength) {
  return prv_set(vibe, strength);
}

static int prv_stop(const struct pbl_vibe *vibe) {
  return prv_set(vibe, 0);
}

static const struct pbl_vibe_ops s_ops = {
  .init = prv_init,
  .start = prv_start,
  .stop = prv_stop,
};

PBL_VIBE_STATE_DEFINE(pbl_vibe_sim);
const struct pbl_vibe pbl_vibe_sim =
    PBL_VIBE_INIT(pbl_vibe_sim, "vibe", nullptr, &s_ops, VIBE_STRENGTH_OFF);
PBL_DEVICE_REGISTER(pbl_vibe_sim, &pbl_vibe_sim.dev);
