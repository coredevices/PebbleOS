/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/pinctrl/sf32lb57.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb57_pinmux.h>

#define NUM_SA_PADS 13U
#define NUM_SB_PADS 13U
#define NUM_PA_PADS 58U

#define PAD_MSK                                                                           \
  (PINMUX_PAD_FSEL_MASK | PINMUX_PAD_PE | PINMUX_PAD_PS | PINMUX_PAD_IE | PINMUX_PAD_SR | \
   PINMUX_PAD_DS_MASK)

static const struct pbl_pinctrl_sf32lb57 *prv_ctrl(const struct pbl_pinctrl_controller *ctrl) {
  return container_of(ctrl, const struct pbl_pinctrl_sf32lb57, ctrl);
}

// Register of a pad, by its index over SA, SB then PA
static int prv_pad_reg(const struct pbl_pinctrl_sf32lb57 *ctrl, uint32_t idx, uintptr_t *reg) {
  if (idx < NUM_SA_PADS) {
    *reg = ctrl->base + SF32LB57_PINMUX_PAD_SA_OFFSET(idx);
  } else if (idx < NUM_SA_PADS + NUM_SB_PADS) {
    *reg = ctrl->base + SF32LB57_PINMUX_PAD_SB_OFFSET(idx - NUM_SA_PADS);
  } else if (idx < NUM_SA_PADS + NUM_SB_PADS + NUM_PA_PADS) {
    *reg = ctrl->base + SF32LB57_PINMUX_PAD_PA_OFFSET(idx - NUM_SA_PADS - NUM_SB_PADS);
  } else {
    return -EINVAL;
  }

  return 0;
}

static int prv_configure_pin(const struct pbl_pinctrl_sf32lb57 *ctrl, uint32_t pin) {
  uint32_t port = PBL_FIELD_GET(PBL_PINCTRL_SF32LB57_PORT_MSK, pin);
  uint32_t pad = PBL_FIELD_GET(PBL_PINCTRL_SF32LB57_PAD_MSK, pin);
  uintptr_t reg;
  int ret;

  switch (port) {
    case PBL_PINCTRL_SF32LB57_PORT_SA:
      ret = pad < NUM_SA_PADS ? prv_pad_reg(ctrl, pad, &reg) : -EINVAL;
      break;
    case PBL_PINCTRL_SF32LB57_PORT_SB:
      ret = pad < NUM_SB_PADS ? prv_pad_reg(ctrl, NUM_SA_PADS + pad, &reg) : -EINVAL;
      break;
    case PBL_PINCTRL_SF32LB57_PORT_PA:
      ret = pad < NUM_PA_PADS ? prv_pad_reg(ctrl, NUM_SA_PADS + NUM_SB_PADS + pad, &reg) : -EINVAL;
      break;
    default:
      ret = -EINVAL;
      break;
  }

  if (ret != 0) {
    return ret;
  }

  // The pad options are encoded where the pad register has them
  pbl_modifyl(PAD_MSK, pin & PAD_MSK, reg);

  return 0;
}

static int prv_configure(const struct pbl_pinctrl_controller *ctrl, const uint32_t *pins,
                         uint8_t num_pins, uintptr_t reg) {
  for (uint8_t i = 0U; i < num_pins; i++) {
    int ret = prv_configure_pin(prv_ctrl(ctrl), pins[i]);

    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

static int prv_get(const struct pbl_pinctrl_controller *ctrl, uint16_t pad, uint32_t *val) {
  uintptr_t reg;
  int ret;

  ret = prv_pad_reg(prv_ctrl(ctrl), pad, &reg);
  if (ret != 0) {
    return ret;
  }

  *val = pbl_readl(reg);

  return 0;
}

static const struct pbl_pinctrl_controller_ops s_ops = {
  .configure = prv_configure,
  .get = prv_get,
};

#define PBL_PINCTRL_SF32LB57_DEFINE(sym, _name, _base)                                            \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                   \
  PBL_DEVICE_INSTANCE(sym, struct pbl_pinctrl_sf32lb57, struct pbl_pinctrl_controller, ctrl) {    \
    .ctrl =                                                                                       \
        PBL_PINCTRL_CONTROLLER_INIT(sym, _name, &s_ops, NUM_SA_PADS + NUM_SB_PADS + NUM_PA_PADS), \
    .base = (_base),                                                                              \
  }

#define PBL_PINCTRL_SF32LB57_DT_STATE(sym, ...)       \
  static const uint32_t sym##_pins[] = {__VA_ARGS__}; \
  const struct pbl_pinctrl_state sym = {              \
    .ctrl = SF32LB57_PINCTRL,                         \
    .pins = sym##_pins,                               \
    .num_pins = ARRAY_LENGTH(sym##_pins),             \
  }

#include PBL_DT_INSTANCES(sifli_sf32lb57_pinctrl_pinctrl_controller)
#include PBL_DT_INSTANCES(sifli_sf32lb57_pinctrl_pinctrl)
