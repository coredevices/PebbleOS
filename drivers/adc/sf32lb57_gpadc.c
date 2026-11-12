/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/adc.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/nvmem.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb57_gpadc.h>
#include <hardware/sf32lb57_hpsys_cfg.h>

#define NUM_CHANNELS 12U
#define NUM_SLOTS    12U

// Conversion timing for its fixed 48 MHz clock, in clock cycles, as the vendor sets it
#define SAMP_WIDTH 95U
#define CONV_WIDTH 100U
#define SAMP_DLY   3U
#define INIT_TIME  8U
// Analog supply of 3.3 V
#define VSP_3V3 2U

#define SETTLE_US  200U
#define CONVERT_US 1000U
// Nominal transfer, two points of it, for a part without factory calibration
#define NOMINAL_RAW_LO 822
#define NOMINAL_UV_LO  0
#define NOMINAL_RAW_HI 3912
#define NOMINAL_UV_HI  3300000

#define CAL_SIZE 8U
#define CAL_MV   100

PBL_LOG_MODULE_DEFINE(adc_sf32lb57_gpadc, CONFIG_ADC_LOG_LEVEL);

// The transfer, as two points: a raw value and its voltage
struct pbl_adc_sf32lb57_gpadc_state {
  int32_t raw_lo;
  int32_t uv_lo;
  int32_t raw_hi;
  int32_t uv_hi;
};

struct pbl_adc_sf32lb57_gpadc {
  struct pbl_adc adc;
  uintptr_t base;
  uintptr_t cfg;
  struct pbl_clk clk;
  struct pbl_nvmem_cell cal;
  struct pbl_adc_sf32lb57_gpadc_state *state;
};

static const struct pbl_adc_sf32lb57_gpadc *prv_gpadc(const struct pbl_adc *adc) {
  return container_of(adc, const struct pbl_adc_sf32lb57_gpadc, adc);
}

static inline uintptr_t prv_reg(const struct pbl_adc_sf32lb57_gpadc *gpadc, uint32_t offset) {
  return gpadc->base + offset;
}

// Power the analog front end up for a conversion, and down after it
static void prv_power(const struct pbl_adc_sf32lb57_gpadc *gpadc, bool on) {
  const uintptr_t anau = gpadc->cfg + SF32LB57_HPSYS_CFG_ANAU_CR_OFFSET;

  if (on) {
    pbl_modifyl(0U, HPSYS_CFG_ANAU_CR_EN_VBAT_MON, anau);
    pbl_modifyl(0U, GPADC_CFG_REG1_LDOREF_EN, prv_reg(gpadc, SF32LB57_GPADC_CFG_REG1_OFFSET));
    pbl_modifyl(0U, GPADC_CFG_REG2_LDO_SW_EN, prv_reg(gpadc, SF32LB57_GPADC_CFG_REG2_OFFSET));
    pbl_modifyl(GPADC_CFG_REG1_MUTE, 0U, prv_reg(gpadc, SF32LB57_GPADC_CFG_REG1_OFFSET));
    pbl_busy_wait_us(SETTLE_US);
    pbl_modifyl(0U, GPADC_CTRL_REG_FRC_EN_ADC, prv_reg(gpadc, SF32LB57_GPADC_CTRL_REG_OFFSET));
    pbl_busy_wait_us(SETTLE_US);
  } else {
    pbl_modifyl(GPADC_CFG_REG2_LDO_SW_EN, 0U, prv_reg(gpadc, SF32LB57_GPADC_CFG_REG2_OFFSET));
    pbl_modifyl(GPADC_CFG_REG1_LDOREF_EN, GPADC_CFG_REG1_MUTE,
                prv_reg(gpadc, SF32LB57_GPADC_CFG_REG1_OFFSET));
    pbl_modifyl(GPADC_CTRL_REG_FRC_EN_ADC, 0U, prv_reg(gpadc, SF32LB57_GPADC_CTRL_REG_OFFSET));
    pbl_modifyl(HPSYS_CFG_ANAU_CR_EN_VBAT_MON, 0U, anau);
  }
}

static int prv_read(const struct pbl_adc *adc, uint32_t channel, int32_t *microvolt) {
  const struct pbl_adc_sf32lb57_gpadc *gpadc = prv_gpadc(adc);
  const uintptr_t irq = prv_reg(gpadc, SF32LB57_GPADC_IRQ_OFFSET);
  int32_t raw;
  int ret;

  uint32_t rdata;

  // Each channel converts in the slot of its number
  pbl_writel(PBL_FIELD_PREP(GPADC_SLOT_PCHNL_MASK, channel) | GPADC_SLOT_EN,
             prv_reg(gpadc, SF32LB57_GPADC_SLOT_OFFSET(channel)));

  prv_power(gpadc, true);

  pbl_writel(GPADC_IRQ_ICR, irq);
  pbl_modifyl(0U, GPADC_CTRL_REG_ADC_START, prv_reg(gpadc, SF32LB57_GPADC_CTRL_REG_OFFSET));
  ret = PBL_POLL_BUSY_US((pbl_readl(irq) & GPADC_IRQ_IRSR) != 0U, 1U, CONVERT_US);
  pbl_writel(GPADC_IRQ_ICR, irq);

  rdata = pbl_readl(prv_reg(gpadc, SF32LB57_GPADC_RDATA_OFFSET(channel / 2U)));
  raw = (int32_t)((channel % 2U) != 0U ? PBL_FIELD_GET(GPADC_RDATA_ODD_MASK, rdata)
                                       : PBL_FIELD_GET(GPADC_RDATA_EVEN_MASK, rdata));

  prv_power(gpadc, false);
  pbl_writel(0U, prv_reg(gpadc, SF32LB57_GPADC_SLOT_OFFSET(channel)));

  if (ret != 0) {
    return ret;
  }

  const struct pbl_adc_sf32lb57_gpadc_state *st = gpadc->state;

  *microvolt = st->uv_lo + (int32_t)((int64_t)(raw - st->raw_lo) * (st->uv_hi - st->uv_lo) /
                                     (st->raw_hi - st->raw_lo));

  return 0;
}

// Two points measured at the factory, packed in the eFuse as the vendor does
static int prv_load_calibration(const struct pbl_adc_sf32lb57_gpadc *gpadc) {
  struct pbl_adc_sf32lb57_gpadc_state *st = gpadc->state;
  uint8_t d[CAL_SIZE];
  int32_t raw_lo, mv_lo, raw_hi, mv_hi;
  int ret;

  if (gpadc->cal.nvmem == nullptr) {
    return -ENOENT;
  }

  ret = pbl_nvmem_cell_read(&gpadc->cal, d, sizeof(d));
  if (ret != 0) {
    return ret;
  }

  raw_lo = (int32_t)d[0] | ((int32_t)(d[1] & 0xfU) << 8);
  mv_lo = ((int32_t)(d[1] >> 4) | ((int32_t)(d[2] & 0x1U) << 4)) * CAL_MV;
  raw_hi = (int32_t)(d[2] >> 1) | ((int32_t)(d[3] & 0x1fU) << 7);
  mv_hi = ((int32_t)(d[3] >> 5) | ((int32_t)(d[4] & 0x3U) << 3)) * CAL_MV;

  if (raw_lo == 0 || mv_lo == 0 || raw_hi <= raw_lo || mv_hi <= mv_lo) {
    return -ENODATA;
  }

  *st = (struct pbl_adc_sf32lb57_gpadc_state){
    .raw_lo = raw_lo,
    .uv_lo = mv_lo * 1000,
    .raw_hi = raw_hi,
    .uv_hi = mv_hi * 1000
  };

  return 0;
}

static int prv_init(const struct pbl_adc *adc) {
  const struct pbl_adc_sf32lb57_gpadc *gpadc = prv_gpadc(adc);
  int ret;

  ret = pbl_device_init(&gpadc->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&gpadc->clk);
  if (ret != 0) {
    return ret;
  }

  ret = prv_load_calibration(gpadc);
  if (ret != 0) {
    PBL_LOG_WRN("No factory calibration (%d), nominal transfer", ret);
    *gpadc->state = (struct pbl_adc_sf32lb57_gpadc_state){
      .raw_lo = NOMINAL_RAW_LO,
      .uv_lo = NOMINAL_UV_LO,
      .raw_hi = NOMINAL_RAW_HI,
      .uv_hi = NOMINAL_UV_HI,
    };
  }

  pbl_modifyl(
      GPADC_CFG_REG1_VSP_MASK | GPADC_CFG_REG1_EN_V18,
      GPADC_CFG_REG1_SE | GPADC_CFG_REG1_MUTE | PBL_FIELD_PREP(GPADC_CFG_REG1_VSP_MASK, VSP_3V3),
      prv_reg(gpadc, SF32LB57_GPADC_CFG_REG1_OFFSET));

  pbl_modifyl(GPADC_CTRL_REG_GPIO_TRIG_EN | GPADC_CTRL_REG_TIMER_TRIG_EN |
                  GPADC_CTRL_REG_INIT_TIME_MASK | GPADC_CTRL_REG_SAMP_DLY_MASK,
              PBL_FIELD_PREP(GPADC_CTRL_REG_INIT_TIME_MASK, INIT_TIME) |
                  PBL_FIELD_PREP(GPADC_CTRL_REG_SAMP_DLY_MASK, SAMP_DLY) |
                  GPADC_CTRL_REG_UNSIGNED_RSLT,
              prv_reg(gpadc, SF32LB57_GPADC_CTRL_REG_OFFSET));
  pbl_writel(PBL_FIELD_PREP(GPADC_CTRL_REG2_SAMP_WIDTH_MASK, SAMP_WIDTH - 1U) |
                 PBL_FIELD_PREP(GPADC_CTRL_REG2_CONV_WIDTH_MASK, CONV_WIDTH - 1U),
             prv_reg(gpadc, SF32LB57_GPADC_CTRL_REG2_OFFSET));

  for (uint32_t slot = 0U; slot < NUM_SLOTS; slot++) {
    pbl_writel(0U, prv_reg(gpadc, SF32LB57_GPADC_SLOT_OFFSET(slot)));
  }

  // The reference runs from the HPSYS bandgap
  pbl_modifyl(0U, HPSYS_CFG_ANAU_CR_EN_BG, gpadc->cfg + SF32LB57_HPSYS_CFG_ANAU_CR_OFFSET);

  return 0;
}

static const struct pbl_adc_ops s_ops = {
  .init = prv_init,
  .read = prv_read,
};

#define PBL_ADC_SF32LB57_GPADC_DEFINE(sym, _name, _base, _cfg, _clk, _cal)                    \
  PBL_ADC_STATE_DEFINE(sym);                                                                  \
  static struct pbl_adc_sf32lb57_gpadc_state sym##_gpadc_state;                               \
  PBL_DEVICE_INSTANCE(sym, struct pbl_adc_sf32lb57_gpadc, struct pbl_adc, adc) {              \
    .adc = PBL_ADC_INIT(sym, _name, nullptr, nullptr, &s_ops, NUM_CHANNELS), .base = (_base), \
    .cfg = (_cfg), .clk = _clk, .cal = _cal, .state = &sym##_gpadc_state,                     \
  }

#include PBL_DT_INSTANCES(sifli_sf32lb57_gpadc)
