/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/pinctrl.h>

PBL_DEVICE_CLASS_DEFINE(pbl_pinctrl_class, "pinctrl");

int pbl_pinctrl_select_state(const struct pbl_pinctrl *pinctrl, enum pbl_pinctrl_state_id id) {
  const struct pbl_pinctrl_state *state;

  if (id >= PBL_PINCTRL_STATE_COUNT) {
    return -EINVAL;
  }

  state = pinctrl->states[id];
  if (state == nullptr) {
    return -ENOENT;
  }

  return state->ctrl->ops->configure(state->ctrl, state->pins, state->num_pins, pinctrl->reg);
}
