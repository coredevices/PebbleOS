/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/pinctrl/sf32lb52.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb52_hpsys_aon.h>
#include <hardware/sf32lb52_memorymap.h>
#include <hardware/sf32lb52_pinmux.h>
#include <hardware/sf32lb52_rtc.h>

#define NUM_SA_PADS 13U
#define NUM_PA_PADS 45U

#define AON_PULL_FIRST_PAD 34U
#define AON_PULL_LAST_PAD  44U
#define AON_PULL_PAWK_BASE 28U
#define PBR_FIRST_PAD      24U
#define PBR_LAST_PAD       27U

// PA39-PA42 have one drive strength bit (4 or 20 mA) and use SR as the I2C mode select
#define FSEL_PBR_CLK_RTC        7U
#define FSEL_PBR_LPTIM1_OUT     8U
#define FSEL_PBR_LPTIM1_INV_OUT 9U
#define FSEL_PBR_LPTIM2_OUT     10U
#define FSEL_PBR_LPTIM2_INV_OUT 11U

#define MODE_FIRST_PAD  39U
#define MODE_LAST_PAD   42U
#define FSEL_I2C        4U
#define PINR_I2C1_FIRST 0x48U
#define PINR_I2C4_LAST  0x54U

#define PAD_MSK                                                                           \
  (PINMUX_PAD_FSEL_MASK | PINMUX_PAD_PE | PINMUX_PAD_PS | PINMUX_PAD_IE | PINMUX_PAD_SR | \
   PINMUX_PAD_DS_MASK)

static const struct pbl_pinctrl_sf32lb52 *prv_ctrl(const struct pbl_pinctrl_controller *ctrl) {
  return container_of(ctrl, const struct pbl_pinctrl_sf32lb52, ctrl);
}

static int prv_pad_value(uint32_t pin, uint32_t pad, bool mode_pad, uint32_t *val) {
  uint32_t ds_idx = PBL_FIELD_GET(PBL_PINCTRL_SF32LB52_DS_MSK, pin);
  uint32_t ds;

  if (mode_pad) {
    if (ds_idx == 4U) {
      ds = 1U;
    } else if (ds_idx == 0U || ds_idx == 2U) {
      ds = 0U;
    } else {
      return -EINVAL;
    }
  } else if (ds_idx == 4U) {
    return -EINVAL;
  } else {
    ds = ds_idx;
  }

  *val = (pin & (PBL_PINCTRL_SF32LB52_FSEL_MSK | PBL_PINCTRL_SF32LB52_PE | PBL_PINCTRL_SF32LB52_PS |
                 PBL_PINCTRL_SF32LB52_IE | PBL_PINCTRL_SF32LB52_SR)) |
         PBL_FIELD_PREP(PINMUX_PAD_DS_MASK, ds);

  if (mode_pad) {
    uint32_t pinr = PBL_FIELD_GET(PBL_PINCTRL_SF32LB52_PINR_MSK, pin);
    bool i2c = PBL_FIELD_GET(PBL_PINCTRL_SF32LB52_FSEL_MSK, pin) == FSEL_I2C &&
               pinr >= PINR_I2C1_FIRST && pinr <= PINR_I2C4_LAST;

    *val &= ~PINMUX_PAD_SR;
    if (i2c) {
      *val |= PINMUX_PAD_SR;
    }
  }

  return 0;
}

static void prv_rtc_pull(uint32_t pad, uint32_t val) {
  bool pe = (val & PINMUX_PAD_PE) != 0U;
  bool ps = (val & PINMUX_PAD_PS) != 0U;

  if (pad >= AON_PULL_FIRST_PAD && pad <= AON_PULL_LAST_PAD) {
    uint32_t bit = RTC_PAWK_PIN(pad - AON_PULL_PAWK_BASE);

    pbl_modifyl(bit, pe ? bit : 0U, SF32LB52_RTC_BASE + SF32LB52_RTC_PAWK1R_OFFSET);
    pbl_modifyl(bit, ps ? bit : 0U, SF32LB52_RTC_BASE + SF32LB52_RTC_PAWK2R_OFFSET);
  } else if (pad >= PBR_FIRST_PAD && pad <= PBR_LAST_PAD) {
    pbl_modifyl(RTC_PBR_PE | RTC_PBR_PS, (pe ? RTC_PBR_PE : 0U) | (ps ? RTC_PBR_PS : 0U),
                SF32LB52_RTC_BASE + SF32LB52_RTC_PBR_OFFSET(pad - PBR_FIRST_PAD));
  }
}

// Backup output of function select fsel, or none for 0
static void prv_pbr_configure(uint32_t pad, uint32_t fsel) {
  uintptr_t reg = SF32LB52_RTC_BASE + SF32LB52_RTC_PBR_OFFSET(pad - PBR_FIRST_PAD);
  uint32_t aon_mask = 0U;
  uint32_t aon_val = 0U;
  uint32_t sel;

  switch (fsel) {
    case 0U:
      pbl_modifyl(RTC_PBR_OE, 0U, reg);
      return;
    case FSEL_PBR_CLK_RTC:
      sel = RTC_PBR_SEL_CLK_RTC;
      break;
    case FSEL_PBR_LPTIM1_OUT:
    case FSEL_PBR_LPTIM2_OUT:
      sel = RTC_PBR_SEL_PINOUT_SEL0;
      aon_mask = HPSYS_AON_CR1_PINOUT_SEL0_MASK;
      aon_val = PBL_FIELD_PREP(aon_mask, (fsel == FSEL_PBR_LPTIM1_OUT)
                                             ? HPSYS_AON_CR1_PINOUT_SEL_LPTIM1_OUT
                                             : HPSYS_AON_CR1_PINOUT_SEL_LPTIM2_OUT);
      break;
    case FSEL_PBR_LPTIM1_INV_OUT:
    case FSEL_PBR_LPTIM2_INV_OUT:
      sel = RTC_PBR_SEL_PINOUT_SEL1;
      aon_mask = HPSYS_AON_CR1_PINOUT_SEL1_MASK;
      aon_val = PBL_FIELD_PREP(aon_mask, (fsel == FSEL_PBR_LPTIM1_INV_OUT)
                                             ? HPSYS_AON_CR1_PINOUT_SEL_LPTIM1_OUT
                                             : HPSYS_AON_CR1_PINOUT_SEL_LPTIM2_OUT);
      break;
    default:
      sel = RTC_PBR_SEL_PBR_OUT;
      break;
  }

  if (aon_mask != 0U) {
    pbl_modifyl(aon_mask, aon_val, SF32LB52_HPSYS_AON_BASE + SF32LB52_HPSYS_AON_CR1_OFFSET);
  }
  pbl_modifyl(RTC_PBR_SEL_MASK, PBL_FIELD_PREP(RTC_PBR_SEL_MASK, sel), reg);
  pbl_modifyl(RTC_PBR_OE, RTC_PBR_OE, reg);
}

static int prv_configure_pin(const struct pbl_pinctrl_sf32lb52 *ctrl, uint32_t pin, uint32_t mask) {
  uint32_t port = PBL_FIELD_GET(PBL_PINCTRL_SF32LB52_PORT_MSK, pin);
  uint32_t pad = PBL_FIELD_GET(PBL_PINCTRL_SF32LB52_PAD_MSK, pin);
  uint32_t pinr = PBL_FIELD_GET(PBL_PINCTRL_SF32LB52_PINR_MSK, pin);
  bool pa = port == PBL_PINCTRL_SF32LB52_PORT_PA;
  bool mode_pad = pa && pad >= MODE_FIRST_PAD && pad <= MODE_LAST_PAD;
  bool pbr = pinr == PBL_PINCTRL_SF32LB52_PINR_PBR;
  uintptr_t reg;
  uint32_t val;
  int ret;

  if (pa) {
    if (pad >= NUM_PA_PADS) {
      return -EINVAL;
    }
    reg = ctrl->pad_pa + pad * 4U;
  } else if (port == PBL_PINCTRL_SF32LB52_PORT_SA) {
    if (pad >= NUM_SA_PADS) {
      return -EINVAL;
    }
    reg = ctrl->pad_sa + pad * 4U;
  } else {
    return -EINVAL;
  }

  ret = prv_pad_value(pin, pad, mode_pad, &val);
  if (ret != 0) {
    return ret;
  }

  if (pbr && (!pa || pad < PBR_FIRST_PAD || pad > PBR_LAST_PAD)) {
    return -EINVAL;
  }

  if (pinr != 0U && !pbr && (mask & PINMUX_PAD_FSEL_MASK) != 0U) {
    uint32_t shift = 8U * PBL_FIELD_GET(PBL_PINCTRL_SF32LB52_FIELD_MSK, pin);

    pbl_modifyl(0xFFU << shift, pad << shift, ctrl->cfg + pinr);
  }

  mask &= PAD_MSK;
  pbl_modifyl(mask, val & mask, reg);

  if (pa && (mask & (PINMUX_PAD_PE | PINMUX_PAD_PS)) != 0U) {
    prv_rtc_pull(pad, val);
  }

  if (pa && pad >= PBR_FIRST_PAD && pad <= PBR_LAST_PAD && (mask & PINMUX_PAD_FSEL_MASK) != 0U) {
    prv_pbr_configure(pad, pbr ? PBL_FIELD_GET(PINMUX_PAD_FSEL_MASK, pin) : 0U);
  }

  return 0;
}

static int prv_configure(const struct pbl_pinctrl_controller *ctrl, const uint32_t *pins,
                         uint8_t num_pins, uintptr_t reg) {
  for (uint8_t i = 0U; i < num_pins; i++) {
    int ret = prv_configure_pin(prv_ctrl(ctrl), pins[i], PAD_MSK);

    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

static int prv_get(const struct pbl_pinctrl_controller *ctrl, uint16_t pad, uint32_t *val) {
  const struct pbl_pinctrl_sf32lb52 *sf32 = prv_ctrl(ctrl);

  if (pad < NUM_SA_PADS) {
    *val = pbl_readl(sf32->pad_sa + pad * 4U);
  } else if (pad < NUM_SA_PADS + NUM_PA_PADS) {
    *val = pbl_readl(sf32->pad_pa + (pad - NUM_SA_PADS) * 4U);
  } else {
    return -EINVAL;
  }

  return 0;
}

int pbl_pinctrl_sf32lb52_modify(uint32_t pin, uint32_t mask) {
  return prv_configure_pin(&pbl_pinctrl_sf32lb52_hpsys, pin, mask);
}

static const struct pbl_pinctrl_controller_ops s_ops = {
  .configure = prv_configure,
  .get = prv_get,
};

PBL_DEVICE_STATE_DEFINE(pbl_pinctrl_sf32lb52_hpsys);
const struct pbl_pinctrl_sf32lb52 pbl_pinctrl_sf32lb52_hpsys = {
  .ctrl = PBL_PINCTRL_CONTROLLER_INIT(pbl_pinctrl_sf32lb52_hpsys, "pinctrl", &s_ops,
                                      NUM_SA_PADS + NUM_PA_PADS),
  .pad_sa = SF32LB52_PINMUX1_BASE + SF32LB52_PINMUX_PAD_SA_OFFSET(0),
  .pad_pa = SF32LB52_PINMUX1_BASE + SF32LB52_PINMUX_PAD_PA_OFFSET(0),
  .cfg = SF32LB52_HPSYS_CFG_BASE,
};
PBL_DEVICE_REGISTER(pbl_pinctrl_sf32lb52_hpsys, &pbl_pinctrl_sf32lb52_hpsys.ctrl.dev);
