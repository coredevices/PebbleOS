/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/adc.h>
#include <pbl/drivers/clk.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32h7_adc.h>

#define NUM_CHANNELS 20U

// Internal channels of ADC3, enabled in the common registers before they are converted
#define CHANNEL_VSENSE  18U
#define CHANNEL_VREFINT 19U

// Highest kernel clock, HCLK divided by 4 for synchronous mode, at the boost of revision V
#define ADC_MAX_HZ 50000000U

// Regulator start-up, then the settling of the temperature sensor, the slower internal channel
#define REGULATOR_US 10U
#define INTERNAL_US  26U

// Longest the linearity calibration and the other steps wait for the ADC
#define TIMEOUT_US 100000U

struct pbl_adc_stm32h7 {
  struct pbl_adc adc;
  // The ADC, and the block holding its common registers
  uintptr_t base;
  uintptr_t block;
  struct pbl_clk clk;
  // The voltage of VREF+, the full scale
  uint32_t vref_uv;
};

static const struct pbl_adc_stm32h7 *prv_adc(const struct pbl_adc *adc) {
  return PBL_CONTAINER_OF(adc, const struct pbl_adc_stm32h7, adc);
}

static inline uint32_t prv_getreg(const struct pbl_adc_stm32h7 *adc, uint32_t offset) {
  return pbl_readl(adc->base + offset);
}

static inline void prv_putreg(const struct pbl_adc_stm32h7 *adc, uint32_t offset, uint32_t value) {
  pbl_writel(value, adc->base + offset);
}

// Wait until the bits are all set, or all cleared
static int prv_wait(const struct pbl_adc_stm32h7 *adc, uint32_t offset, uint32_t bits, bool set) {
  for (uint32_t us = 0U; us < TIMEOUT_US; us++) {
    if (((prv_getreg(adc, offset) & bits) == bits) == set) {
      return 0;
    }
    pbl_busy_wait_us(1U);
  }

  return -ETIMEDOUT;
}

static int prv_enable(const struct pbl_adc_stm32h7 *adc) {
  prv_putreg(adc, STM32H7_ADC_ISR_OFFSET, ADC_ISR_ADRDY);
  pbl_modifyl(0U, ADC_CR_ADEN, adc->base + STM32H7_ADC_CR_OFFSET);

  return prv_wait(adc, STM32H7_ADC_ISR_OFFSET, ADC_ISR_ADRDY, true);
}

static void prv_disable(const struct pbl_adc_stm32h7 *adc) {
  pbl_modifyl(0U, ADC_CR_ADDIS, adc->base + STM32H7_ADC_CR_OFFSET);
  (void)prv_wait(adc, STM32H7_ADC_CR_OFFSET, ADC_CR_ADEN, false);
}

static uint32_t prv_internal(uint32_t channel) {
  switch (channel) {
    case CHANNEL_VSENSE:
      return ADC_CCR_TSEN;
    case CHANNEL_VREFINT:
      return ADC_CCR_VREFEN;
    default:
      return 0U;
  }
}

// One conversion at the longest sampling time, 810.5 cycles, which the internal channels need
static int prv_convert(const struct pbl_adc_stm32h7 *adc, uint32_t channel, uint32_t *raw) {
  const uint32_t internal = prv_internal(channel);
  const uint32_t smpr = (channel < 10U) ? STM32H7_ADC_SMPR1_OFFSET : STM32H7_ADC_SMPR2_OFFSET;
  const uint32_t shift = (channel % 10U) * ADC_SMPR_SMP_BITS;
  int ret;

  if (internal != 0U) {
    pbl_modifyl(0U, internal, adc->block + STM32H7_ADC_CCR_OFFSET);
    pbl_busy_wait_us(INTERNAL_US);
  }

  pbl_modifyl(PBL_GENMASK(shift + ADC_SMPR_SMP_BITS - 1U, shift), ADC_SMPR_SMP_MAX << shift,
              adc->base + smpr);
  prv_putreg(adc, STM32H7_ADC_PCSEL_OFFSET, PBL_BIT(channel));
  prv_putreg(adc, STM32H7_ADC_SQR1_OFFSET, PBL_FIELD_PREP(ADC_SQR1_SQ1_MASK, channel));

  ret = prv_enable(adc);
  if (ret == 0) {
    prv_putreg(adc, STM32H7_ADC_ISR_OFFSET, ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR);
    pbl_modifyl(0U, ADC_CR_ADSTART, adc->base + STM32H7_ADC_CR_OFFSET);
    ret = prv_wait(adc, STM32H7_ADC_ISR_OFFSET, ADC_ISR_EOC, true);
    *raw = prv_getreg(adc, STM32H7_ADC_DR_OFFSET);
  }
  prv_disable(adc);

  if (internal != 0U) {
    pbl_modifyl(internal, 0U, adc->block + STM32H7_ADC_CCR_OFFSET);
  }

  return ret;
}

static int prv_read(const struct pbl_adc *dev, uint32_t channel, int32_t *microvolt) {
  const struct pbl_adc_stm32h7 *adc = prv_adc(dev);
  uint32_t raw = 0U;
  int ret;

  ret = pbl_clk_enable(&adc->clk);
  if (ret != 0) {
    return ret;
  }

  ret = prv_convert(adc, channel, &raw);
  (void)pbl_clk_disable(&adc->clk);
  if (ret != 0) {
    return ret;
  }

  *microvolt = (int32_t)(((uint64_t)raw * adc->vref_uv) / UINT16_MAX);

  return 0;
}

// Out of deep power-down, with its regulator on and calibrated, the ADC keeps its calibration
// while disabled between conversions
static int prv_power_up(const struct pbl_adc_stm32h7 *adc) {
  pbl_modifyl(ADC_CR_DEEPPWD, ADC_CR_ADVREGEN, adc->base + STM32H7_ADC_CR_OFFSET);
  pbl_busy_wait_us(REGULATOR_US);

  pbl_modifyl(ADC_CR_BOOST_MASK, ADC_CR_BOOST_MASK, adc->base + STM32H7_ADC_CR_OFFSET);
  prv_putreg(adc, STM32H7_ADC_CFGR_OFFSET, ADC_CFGR_JQDIS | ADC_CFGR_OVRMOD);

  pbl_modifyl(ADC_CR_ADCALDIF, ADC_CR_ADCALLIN | ADC_CR_ADCAL, adc->base + STM32H7_ADC_CR_OFFSET);

  return prv_wait(adc, STM32H7_ADC_CR_OFFSET, ADC_CR_ADCAL, false);
}

static int prv_init(const struct pbl_adc *dev) {
  const struct pbl_adc_stm32h7 *adc = prv_adc(dev);
  uint64_t rate_mhz;
  int ret;

  ret = pbl_device_init(&adc->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_get_rate(&adc->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  if (rate_mhz / 4000U > ADC_MAX_HZ || adc->vref_uv == 0U) {
    return -EINVAL;
  }

  ret = pbl_clk_enable(&adc->clk);
  if (ret != 0) {
    return ret;
  }

  pbl_modifyl(ADC_CCR_CKMODE_MASK, PBL_FIELD_PREP(ADC_CCR_CKMODE_MASK, ADC_CCR_CKMODE_HCLK4),
              adc->block + STM32H7_ADC_CCR_OFFSET);
  ret = prv_power_up(adc);
  (void)pbl_clk_disable(&adc->clk);

  return ret;
}

static const struct pbl_adc_ops s_ops = {
  .init = prv_init,
  .read = prv_read,
};

#define PBL_ADC_STM32H7_DEFINE(sym, _name, _block, _offset, _clk, _vref_uv)                       \
  PBL_ADC_STATE_DEFINE(sym);                                                                      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_adc_stm32h7, struct pbl_adc, adc) {                         \
    .adc = PBL_ADC_INIT(sym, _name, nullptr, &s_ops, NUM_CHANNELS), .base = (_block) + (_offset), \
    .block = (_block), .clk = _clk, .vref_uv = (_vref_uv),                                        \
  }

#include PBL_DT_INSTANCES(st_stm32h7_adc_adc)
