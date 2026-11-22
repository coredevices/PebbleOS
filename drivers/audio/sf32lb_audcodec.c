/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/audio/sf32lb.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/dma/sf32lb.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/soc/sf32lb/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/sf32lb52_audcodec.h>

#ifdef CONFIG_SOC_SF32LB57
#include <hardware/sf32lb57_audcodec.h>
#include <hardware/sf32lb57_pmuc.h>

#define ANA(name)           SF32LB57_AUDCODEC_##name##_OFFSET
#define PMUC_HXT_CR1_OFFSET SF32LB57_PMUC_HXT_CR1_OFFSET
#define PLL_CFG4_EN_CLK_DIG SF32LB57_AUDCODEC_PLL_CFG4_EN_CLK_DIG
#define PLL_CFG4_CLK_DIG \
  (PLL_CFG4_EN_CLK_DIG | PBL_FIELD_PREP(SF32LB57_AUDCODEC_PLL_CFG4_DIVB_CLK_DIG_MASK, 2U))
#define ADCN_CFG1_VCMST SF32LB57_AUDCODEC_ADCN_CFG1_VCMST
#define PLL_ICP_SEL     3U
#define HAS_DAC2        false
#else
#include <hardware/sf32lb52_pmuc.h>

#define ANA(name)           SF32LB52_AUDCODEC_##name##_OFFSET
#define PMUC_HXT_CR1_OFFSET SF32LB52_PMUC_HXT_CR1_OFFSET
#define PLL_CFG4_EN_CLK_DIG AUDCODEC_PLL_CFG4_EN_CLK_DIG
#define PLL_CFG4_CLK_DIG    PLL_CFG4_EN_CLK_DIG
#define ADCN_CFG1_VCMST     AUDCODEC_ADCN_CFG1_VCMST
#define PLL_ICP_SEL         8U
#define HAS_DAC2            true
#endif

PBL_LOG_MODULE_DECLARE(driver_audio, CONFIG_DRIVER_AUDIO_LOG_LEVEL);

static_assert(PBL_DCACHE_LINE_SIZE <= PBL_AUDIO_BUF_ALIGN,
              "ring buffers must own their cache lines");

#define REG(name) (dev->base + SF32LB52_AUDCODEC_##name##_OFFSET)
// Analog registers, at offsets of the series
#define AREG(name) (dev->base + ANA(name))

#define GAIN_MIN_MDB  (-36000)
#define GAIN_MAX_MDB  0
#define GAIN_STEP_MDB 500
// Fine volume steps per rough volume step
#define FINE_STEPS    12U
#define ROUGH_VOL_0DB ((GAIN_MAX_MDB - GAIN_MIN_MDB) / GAIN_STEP_MDB / FINE_STEPS)
// Fine volume of a muted output
#define FINE_VOL_MUTE 0xfU

#define ADC_GAIN_MIN_MDB     (-60000)
#define ADC_GAIN_MAX_MDB     30000
#define ADC_GAIN_DEFAULT_MDB 12000

// Digital output while the DAC path is bypassed
#define DEBUG_DATA_OUT 0xffU

// DAC configuration for a 3.3 V AVDD
#define DAC_OP_MODE    1U
#define DAC_DEM_MODE   2U
#define DAC_SINC_GAIN  0x14dU
#define DAC_RAMP_STEPS 2U
#define BG_VREF_3V3    0xcU
#define BG_MIC_VREF    4U
#define CFG_EN_DLY     3U

#define ADC_OP_MODE       1U
#define ADC_HPF_COEF      7U
#define ADC_GC            4U
#define ADC_VREF          2U
#define ADC_DIVA_CLK      5U
#define ADC0_SEL_CLK      1U
#define ADC0_DIVA_CLK     5U
#define MICBIAS_SEL_CLK   3U
#define MICBIAS_SETTLE_MS 2U
#define ADC_RESET_MS      1U
#define ADC_VCM_SETTLE_MS 20U

// The second ADC of the SF32LB52 sets its gain in a layout of its own, not handled yet
#ifdef CONFIG_SOC_SF32LB57
#define ADC_CHANNELS_MAX 2U
#else
#define ADC_CHANNELS_MAX 1U
#endif

#define PLL_R3_SEL 3U
#define PLL_RZ_SEL 1U
#define PLL_C2_SEL 3U
#define PLL_CZ_SEL 6U
// 44.1 MHz: (FCW + 3 + SDIN / 2^20) * 6 MHz
#define PLL_44M1_SDIN 0x5999aU
#define PLL_44M1_FCW  4U

// VCO band search: start in the middle of the 5-bit range, halving the step
#define FC_VCO_START 16U
#define FC_VCO_STEP  8U
#define FC_VCO_MAX   31U
#define CAL_LEN      2000U
#define CAL_POLL_US  1000U

#define PLL_LOCK_TRIES 10U

// Rate the DAC channel is set up for until the first stream
#define INIT_RATE 16000U

#define BG_SETTLE_US     100U
#define PLL_SETTLE_US    50U
#define REFGEN_SETTLE_US 2000U
#define PLL_RESET_US     100U
#define DAC_VCM_US       5U
#define DAC_AMP_US       1U
#define DAC_STEP_US      10U

enum pll_state {
  PLL_OFF,
  PLL_BIAS,
  PLL_LOCKED,
};

enum vco {
  VCO_48M,
  VCO_44M1,
};

// Count the VCO is calibrated to, for the 48 MHz and the 44.1 MHz families
static const uint32_t s_vco_target[] = {
  [VCO_48M] = 2001U,
  [VCO_44M1] = 1834U,
};

struct prv_rate {
  uint32_t rate;
  bool pll;
  uint8_t clk_div;
  uint8_t osr_sel;
  bool sel_clk_dac;
  uint8_t adc_clk_div;
  uint8_t adc_osr_sel;
  bool sel_clk_adc;
  uint8_t adc_fsp;
};

static const struct prv_rate s_rates[] = {
  {48000U, false, 1U, 0U, false, 5U, 0U, true, 0U},
  {32000U, false, 1U, 1U, false, 5U, 1U, true, 0U},
  {24000U, false, 1U, 5U, true, 10U, 0U, false, 2U},
  {16000U, false, 1U, 4U, false, 10U, 1U, false, 2U},
  {12000U, false, 1U, 7U, true, 10U, 2U, false, 2U},
  {8000U, false, 1U, 8U, true, 10U, 3U, false, 2U},
  {44100U, true, 1U, 0U, false, 5U, 0U, true, 1U},
  {22050U, true, 1U, 5U, true, 5U, 2U, true, 1U},
  {11025U, true, 1U, 7U, true, 10U, 2U, false, 3U},
};

static const struct pbl_audio_sf32lb_audcodec *prv_dev(const struct pbl_audio_dai *dai) {
  return PBL_CONTAINER_OF(dai - dai->dir, const struct pbl_audio_sf32lb_audcodec, dai);
}

static inline uint32_t prv_read(uintptr_t reg) {
  return pbl_readl(reg);
}

static inline void prv_write(uintptr_t reg, uint32_t value) {
  pbl_writel(value, reg);
}

static inline void prv_set(uintptr_t reg, uint32_t bits) {
  pbl_writel(pbl_readl(reg) | bits, reg);
}

static inline void prv_clear(uintptr_t reg, uint32_t bits) {
  pbl_writel(pbl_readl(reg) & ~bits, reg);
}

static inline void prv_field(uintptr_t reg, uint32_t mask, uint32_t value) {
  pbl_writel((pbl_readl(reg) & ~mask) | PBL_FIELD_PREP(mask, value), reg);
}

static const struct prv_rate *prv_find_rate(uint32_t rate) {
  for (size_t i = 0U; i < PBL_ARRAY_SIZE(s_rates); i++) {
    if (s_rates[i].rate == rate) {
      return &s_rates[i];
    }
  }

  return nullptr;
}

static void prv_get_rates(struct pbl_audio_caps *caps) {
  for (size_t i = 0U; i < PBL_ARRAY_SIZE(s_rates); i++) {
    caps->rates |= pbl_audio_rate_bit(s_rates[i].rate);
  }
}

static void prv_refgen_on(const struct pbl_audio_sf32lb_audcodec *dev) {
  prv_clear(REG(BG_CFG0), AUDCODEC_BG_CFG0_EN_SMPL);
  prv_clear(REG(REFGEN_CFG), AUDCODEC_REFGEN_CFG_EN_CHOP);
  prv_set(REG(REFGEN_CFG), AUDCODEC_REFGEN_CFG_EN);
  prv_clear(REG(REFGEN_CFG), AUDCODEC_REFGEN_CFG_LV_MODE);
  prv_set(REG(PLL_CFG5), AUDCODEC_PLL_CFG5_EN_CLK_CHOP_BG);
  prv_set(REG(PLL_CFG5), AUDCODEC_PLL_CFG5_EN_CLK_CHOP_REFGEN);
  pbl_busy_wait_us(REFGEN_SETTLE_US);
  prv_clear(REG(BG_CFG0), AUDCODEC_BG_CFG0_EN_SMPL);
}

// Bandgap, reference generator and PLL analog: the bias of every analog block
static void prv_bias_on(const struct pbl_audio_sf32lb_audcodec *dev) {
  prv_write(REG(BG_CFG0), AUDCODEC_BG_CFG0_EN |
                              PBL_FIELD_PREP(AUDCODEC_BG_CFG0_VREF_SEL_MASK, BG_VREF_3V3) |
                              AUDCODEC_BG_CFG0_EN_RCFLT |
                              PBL_FIELD_PREP(AUDCODEC_BG_CFG0_MIC_VREF_SEL_MASK, BG_MIC_VREF) |
                              AUDCODEC_BG_CFG0_EN_AMP);
  prv_write(REG(BG_CFG1), 0U);
  prv_write(REG(BG_CFG2), 0U);
  pbl_busy_wait_us(BG_SETTLE_US);

  prv_set(REG(PLL_CFG0),
          AUDCODEC_PLL_CFG0_EN_IARY | AUDCODEC_PLL_CFG0_EN_VCO | AUDCODEC_PLL_CFG0_EN_ANA);
  prv_field(REG(PLL_CFG0), AUDCODEC_PLL_CFG0_ICP_SEL_MASK, PLL_ICP_SEL);
  prv_set(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_EN_DIG);
  prv_set(REG(PLL_CFG3), AUDCODEC_PLL_CFG3_EN_SDM);
  prv_set(REG(PLL_CFG4), PLL_CFG4_CLK_DIG);
  prv_write(REG(PLL_CFG1), PBL_FIELD_PREP(AUDCODEC_PLL_CFG1_R3_SEL_MASK, PLL_R3_SEL) |
                               PBL_FIELD_PREP(AUDCODEC_PLL_CFG1_RZ_SEL_MASK, PLL_RZ_SEL) |
                               PBL_FIELD_PREP(AUDCODEC_PLL_CFG1_C2_SEL_MASK, PLL_C2_SEL) |
                               PBL_FIELD_PREP(AUDCODEC_PLL_CFG1_CZ_SEL_MASK, PLL_CZ_SEL));
  pbl_busy_wait_us(PLL_SETTLE_US);

  prv_refgen_on(dev);
}

static void prv_bias_off(const struct pbl_audio_sf32lb_audcodec *dev) {
  prv_clear(REG(PLL_CFG0),
            AUDCODEC_PLL_CFG0_EN_IARY | AUDCODEC_PLL_CFG0_EN_VCO | AUDCODEC_PLL_CFG0_EN_ANA);
  prv_clear(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_EN_DIG);
  prv_clear(REG(PLL_CFG3), AUDCODEC_PLL_CFG3_EN_SDM);
  prv_clear(REG(PLL_CFG4), PLL_CFG4_EN_CLK_DIG);
  prv_clear(REG(REFGEN_CFG), AUDCODEC_REFGEN_CFG_EN);
  prv_write(REG(BG_CFG1), 0U);
  prv_write(REG(BG_CFG2), 0U);
  prv_clear(REG(BG_CFG0), AUDCODEC_BG_CFG0_EN);
  prv_clear(REG(BG_CFG0), AUDCODEC_BG_CFG0_EN_SMPL);
}

static int prv_cal_count(const struct pbl_audio_sf32lb_audcodec *dev, uint32_t fc_vco,
                         uint32_t *count) {
  int ret;

  prv_field(REG(PLL_CFG0), AUDCODEC_PLL_CFG0_FC_VCO_MASK, fc_vco);
  prv_set(REG(PLL_CAL_CFG), AUDCODEC_PLL_CAL_CFG_EN);
  ret = PBL_POLL_BUSY_US((prv_read(REG(PLL_CAL_CFG)) & AUDCODEC_PLL_CAL_CFG_DONE) != 0U, 0U,
                         CAL_POLL_US);
  *count = PBL_FIELD_GET(AUDCODEC_PLL_CAL_RESULT_PLL_CNT_MASK, prv_read(REG(PLL_CAL_RESULT)));
  prv_clear(REG(PLL_CAL_CFG), AUDCODEC_PLL_CAL_CFG_EN);

  return ret;
}

static void prv_update_delta(uint32_t count, uint32_t target, uint32_t *delta) {
  if (count < target) {
    *delta = target - count;
  } else if (count > target) {
    *delta = count - target;
  }
}

static int prv_cal_vco(const struct pbl_audio_sf32lb_audcodec *dev, uint32_t target,
                       uint32_t *deltas, uint32_t *vco) {
  uint32_t fc_vco = FC_VCO_START;
  uint32_t fc_min;
  uint32_t fc_max;
  uint32_t count;
  int ret;

  for (uint32_t step = FC_VCO_STEP; step != 0U; step >>= 1) {
    ret = prv_cal_count(dev, fc_vco, &count);
    if (ret != 0) {
      return ret;
    }

    if (count < target) {
      fc_vco += step;
    } else if (count > target) {
      fc_vco -= step;
    }
    prv_update_delta(count, target, &deltas[0]);
  }

  fc_min = fc_vco == 0U ? 0U : fc_vco - 1U;
  fc_max = fc_vco == FC_VCO_MAX ? fc_vco : fc_vco + 1U;

  ret = prv_cal_count(dev, fc_min, &count);
  if (ret != 0) {
    return ret;
  }
  prv_update_delta(count, target, &deltas[1]);

  ret = prv_cal_count(dev, fc_max, &count);
  if (ret != 0) {
    return ret;
  }
  prv_update_delta(count, target, &deltas[2]);

  if (deltas[1] <= deltas[0] && deltas[1] <= deltas[2]) {
    *vco = fc_min;
  } else if (deltas[2] <= deltas[0] && deltas[2] <= deltas[1]) {
    *vco = fc_max;
  } else {
    *vco = fc_vco;
  }

  return 0;
}

// Find the VCO band closest to each PLL frequency family against the crystal
static int prv_calibrate(const struct pbl_audio_sf32lb_audcodec *dev) {
  uint32_t deltas[3] = {0U};
  int ret = 0;

  prv_bias_on(dev);

  prv_set(REG(PLL_CFG0), AUDCODEC_PLL_CFG0_OPEN);
  prv_set(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_EN_LF_VCIN);
  prv_write(REG(PLL_CAL_CFG), PBL_FIELD_PREP(AUDCODEC_PLL_CAL_CFG_LEN_MASK, CAL_LEN));

  for (size_t i = 0U; i < PBL_ARRAY_SIZE(s_vco_target) && ret == 0; i++) {
    ret = prv_cal_vco(dev, s_vco_target[i], deltas, &dev->state->vco[i]);
  }

  prv_clear(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_EN_LF_VCIN);
  prv_clear(REG(PLL_CFG0), AUDCODEC_PLL_CFG0_OPEN);

  prv_bias_off(dev);

  return ret;
}

static int prv_pll_lock_44m1(const struct pbl_audio_sf32lb_audcodec *dev) {
  prv_set(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_RSTB);
  pbl_busy_wait_us(PLL_SETTLE_US);

  prv_write(REG(PLL_CFG3), PBL_FIELD_PREP(AUDCODEC_PLL_CFG3_SDIN_MASK, PLL_44M1_SDIN) |
                               PBL_FIELD_PREP(AUDCODEC_PLL_CFG3_FCW_MASK, PLL_44M1_FCW) |
                               AUDCODEC_PLL_CFG3_SDMIN_BYPASS | AUDCODEC_PLL_CFG3_EN_SDM);
  prv_set(REG(PLL_CFG3), AUDCODEC_PLL_CFG3_SDM_UPDATE);
  prv_clear(REG(PLL_CFG3), AUDCODEC_PLL_CFG3_SDMIN_BYPASS);
  prv_clear(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_RSTB);
  pbl_busy_wait_us(PLL_SETTLE_US);
  prv_set(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_RSTB);
  pbl_busy_wait_us(PLL_SETTLE_US);

  prv_set(REG(PLL_CFG1), AUDCODEC_PLL_CFG1_CSD_EN | AUDCODEC_PLL_CFG1_CSD_RST);
  pbl_busy_wait_us(PLL_SETTLE_US);
  prv_clear(REG(PLL_CFG1), AUDCODEC_PLL_CFG1_CSD_RST);
  if ((prv_read(REG(PLL_STAT)) & AUDCODEC_PLL_STAT_UNLOCK) != 0U) {
    return -EIO;
  }

  prv_clear(REG(PLL_CFG1), AUDCODEC_PLL_CFG1_CSD_EN);

  return 0;
}

static int prv_clock_on(const struct pbl_audio_sf32lb_audcodec *dev, const struct prv_rate *rate) {
  struct pbl_audio_sf32lb_audcodec_state *state = dev->state;
  int ret = -EIO;

  if (state->pll == PLL_OFF) {
    prv_bias_on(dev);
    state->pll = PLL_BIAS;
  }

  if (!rate->pll || state->pll == PLL_LOCKED) {
    return 0;
  }

  prv_field(REG(PLL_CFG0), AUDCODEC_PLL_CFG0_FC_VCO_MASK, state->vco[VCO_44M1]);
  for (uint32_t i = 0U; i < PLL_LOCK_TRIES && ret != 0; i++) {
    ret = prv_pll_lock_44m1(dev);
  }
  if (ret != 0) {
    PBL_LOG_ERR("PLL did not lock");
    return ret;
  }

  state->pll = PLL_LOCKED;

  return 0;
}

// Powers the bias and the PLL down once no direction streams
static void prv_clock_put(const struct pbl_audio_sf32lb_audcodec *dev) {
  if (dev->state->running == 0U) {
    prv_bias_off(dev);
    dev->state->pll = PLL_OFF;
  }
}

// Resets the clock dividers, unless the other direction streams from them
static void prv_clock_reset(const struct pbl_audio_sf32lb_audcodec *dev, uint8_t dir, uint32_t us) {
  if ((dev->state->running & ~PBL_BIT(dir)) != 0U) {
    return;
  }

  prv_clear(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_RSTB);
  pbl_busy_wait_us(us);
  prv_set(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_RSTB);
}

static void prv_dma_done(void *arg, int status) {
  const struct pbl_audio_dai *dai = arg;

  if (status != 0) {
    pbl_audio_report_xrun(pbl_audio_dai_card(dai));
    return;
  }

  pbl_audio_period_elapsed(pbl_audio_dai_card(dai));
}

static int prv_pointer(const struct pbl_audio_dai *dai, const struct pbl_dma_chan *dma,
                       uint32_t ring_bytes, size_t *frames) {
  const struct pbl_audio_state *state = pbl_audio_dai_card(dai)->state;
  size_t residue;
  int ret;

  ret = pbl_dma_tx_status(dma, &residue);
  if (ret != 0) {
    return ret;
  }

  *frames = ((ring_bytes - residue) % ring_bytes) / (state->stream.channels * sizeof(int16_t));

  return 0;
}

/* DAC */

static void prv_dac_config(const struct pbl_audio_sf32lb_audcodec *dev,
                           const struct prv_rate *rate) {
  prv_field(REG(CFG), AUDCODEC_CFG_ADC_EN_DLY_SEL_MASK, CFG_EN_DLY);
  prv_write(REG(DAC_CFG), PBL_FIELD_PREP(AUDCODEC_DAC_CFG_OSR_SEL_MASK, rate->osr_sel) |
                              PBL_FIELD_PREP(AUDCODEC_DAC_CFG_OP_MODE_MASK, DAC_OP_MODE) |
                              (rate->pll ? AUDCODEC_DAC_CFG_CLK_SRC_SEL : 0U) |
                              PBL_FIELD_PREP(AUDCODEC_DAC_CFG_CLK_DIV_MASK, rate->clk_div));
  prv_write(REG(DAC_CH0_CFG),
            AUDCODEC_DAC_CH_CFG_ENABLE |
                PBL_FIELD_PREP(AUDCODEC_DAC_CH_CFG_DEM_MODE_MASK, DAC_DEM_MODE) |
                PBL_FIELD_PREP(AUDCODEC_DAC_CH_CFG_ROUGH_VOL_MASK, ROUGH_VOL_0DB) |
                AUDCODEC_DAC_CH_CFG_DATA_FORMAT |
                PBL_FIELD_PREP(AUDCODEC_DAC_CH_CFG_SINC_GAIN_MASK, DAC_SINC_GAIN));
  prv_write(REG(DAC_CH0_CFG_EXT),
            AUDCODEC_DAC_CH_CFG_EXT_RAMP_EN | AUDCODEC_DAC_CH_CFG_EXT_RAMP_MODE |
                AUDCODEC_DAC_CH_CFG_EXT_ZERO_ADJUST_EN |
                PBL_FIELD_PREP(AUDCODEC_DAC_CH_CFG_EXT_RAMP_INTERVAL_MASK, DAC_RAMP_STEPS));
  prv_write(REG(DAC_CH0_DEBUG),
            PBL_FIELD_PREP(AUDCODEC_DAC_CH_DEBUG_DATA_OUT_MASK, DEBUG_DATA_OUT));
}

static void prv_bypass(const struct pbl_audio_sf32lb_audcodec *dev, bool bypass) {
  uint32_t debug = PBL_FIELD_PREP(AUDCODEC_DAC_CH_DEBUG_DATA_OUT_MASK, DEBUG_DATA_OUT) |
                   (bypass ? AUDCODEC_DAC_CH_DEBUG_BYPASS : 0U);

  if (bypass) {
    prv_set(REG(DAC_CH0_CFG), AUDCODEC_DAC_CH_CFG_DOUT_MUTE);
    prv_write(REG(DAC_CH0_DEBUG), debug);
    if (HAS_DAC2) {
      prv_set(REG(DAC_CH1_CFG), AUDCODEC_DAC_CH_CFG_DOUT_MUTE);
      prv_write(REG(DAC_CH1_DEBUG), debug);
    }
  } else {
    prv_clear(REG(DAC_CH0_CFG), AUDCODEC_DAC_CH_CFG_DOUT_MUTE);
    prv_write(REG(DAC_CH0_DEBUG), debug);
    if (HAS_DAC2) {
      prv_clear(REG(DAC_CH1_CFG), AUDCODEC_DAC_CH_CFG_DOUT_MUTE);
      prv_write(REG(DAC_CH1_DEBUG), debug);
    }
  }
}

// Gain is -36 dB plus rough steps of 6 dB and fine steps of 0.5 dB
static void prv_set_volume(const struct pbl_audio_sf32lb_audcodec *dev, int32_t gain_mdb) {
  uint32_t steps = (uint32_t)pbl_audio_gain_to_step(gain_mdb, GAIN_MIN_MDB, GAIN_STEP_MDB);
  uint32_t mask = AUDCODEC_DAC_CH_CFG_ROUGH_VOL_MASK | AUDCODEC_DAC_CH_CFG_FINE_VOL_MASK;
  uint32_t value = PBL_FIELD_PREP(AUDCODEC_DAC_CH_CFG_ROUGH_VOL_MASK, steps / FINE_STEPS) |
                   PBL_FIELD_PREP(AUDCODEC_DAC_CH_CFG_FINE_VOL_MASK, steps % FINE_STEPS);

  pbl_writel((prv_read(REG(DAC_CH0_CFG)) & ~mask) | value, REG(DAC_CH0_CFG));
  if (HAS_DAC2) {
    pbl_writel((prv_read(REG(DAC_CH1_CFG)) & ~mask) | value, REG(DAC_CH1_CFG));
  }
}

static void prv_apply(const struct pbl_audio_sf32lb_audcodec *dev, int32_t gain_mdb, bool mute) {
  if (mute) {
    prv_bypass(dev, true);
    prv_field(REG(DAC_CH0_CFG), AUDCODEC_DAC_CH_CFG_FINE_VOL_MASK, FINE_VOL_MUTE);
    if (HAS_DAC2) {
      prv_field(REG(DAC_CH1_CFG), AUDCODEC_DAC_CH_CFG_FINE_VOL_MASK, FINE_VOL_MUTE);
    }
    return;
  }

  prv_bypass(dev, false);
  prv_set_volume(dev, gain_mdb);
}

static void prv_dac_analog_on(const struct pbl_audio_sf32lb_audcodec *dev,
                              const struct prv_rate *rate) {
  prv_clear(REG(PLL_CFG4), AUDCODEC_PLL_CFG4_SEL_CLK_DAC | AUDCODEC_PLL_CFG4_SEL_CLK_DAC_SOURCE);
  prv_set(REG(PLL_CFG4), AUDCODEC_PLL_CFG4_EN_CLK_CHOP_DAC | AUDCODEC_PLL_CFG4_EN_CLK_DAC |
                             (rate->pll ? AUDCODEC_PLL_CFG4_SEL_CLK_DAC_SOURCE : 0U) |
                             (rate->sel_clk_dac ? AUDCODEC_PLL_CFG4_SEL_CLK_DAC : 0U) |
                             PLL_CFG4_CLK_DIG);
  prv_set(REG(PLL_CFG5), AUDCODEC_PLL_CFG5_EN_CLK_CHOP_BG | AUDCODEC_PLL_CFG5_EN_CLK_CHOP_REFGEN);

  prv_clock_reset(dev, PBL_AUDIO_PLAYBACK, PLL_RESET_US);

  prv_clear(AREG(DAC1_CFG), AUDCODEC_DACN_CFG_LP_MODE);
  prv_clear(AREG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_OS_DAC);
  // The second output stays off, its offset DAC on
  if (HAS_DAC2) {
    prv_clear(REG(DAC2_CFG), AUDCODEC_DACN_CFG_EN_OS_DAC | AUDCODEC_DACN_CFG_EN_VCM |
                                 AUDCODEC_DACN_CFG_EN_AMP | AUDCODEC_DACN_CFG_EN_DAC);
  }
  prv_set(AREG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_VCM);
  pbl_busy_wait_us(DAC_VCM_US);
  prv_set(AREG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_AMP);
  pbl_busy_wait_us(DAC_AMP_US);
  prv_set(AREG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_OS_DAC);
  if (HAS_DAC2) {
    prv_set(REG(DAC2_CFG), AUDCODEC_DACN_CFG_EN_OS_DAC);
  }
  pbl_busy_wait_us(DAC_STEP_US);
  prv_set(AREG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_DAC);
  pbl_busy_wait_us(DAC_STEP_US);
  prv_clear(AREG(DAC1_CFG), AUDCODEC_DACN_CFG_SR);
  if (HAS_DAC2) {
    prv_clear(REG(DAC2_CFG), AUDCODEC_DACN_CFG_SR);
  }
}

static void prv_dac_analog_off(const struct pbl_audio_sf32lb_audcodec *dev) {
  prv_set(AREG(DAC1_CFG), AUDCODEC_DACN_CFG_SR);
  pbl_busy_wait_us(DAC_STEP_US);
  prv_clear(AREG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_DAC);
  pbl_busy_wait_us(DAC_STEP_US);
  prv_clear(AREG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_VCM);
  pbl_busy_wait_us(DAC_STEP_US);
  prv_clear(AREG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_AMP | AUDCODEC_DACN_CFG_EN_OS_DAC);
  if (HAS_DAC2) {
    prv_set(REG(DAC2_CFG), AUDCODEC_DACN_CFG_SR);
    prv_clear(REG(DAC2_CFG), AUDCODEC_DACN_CFG_EN_DAC | AUDCODEC_DACN_CFG_EN_VCM |
                                 AUDCODEC_DACN_CFG_EN_AMP | AUDCODEC_DACN_CFG_EN_OS_DAC);
  }
}

static void prv_dac_get_caps(const struct pbl_audio_dai *dai, struct pbl_audio_caps *caps) {
  prv_get_rates(caps);
  caps->min_channels = 1U;
  caps->max_channels = 1U;
  caps->max_periods = 2U;
  caps->gain_min_mdb = GAIN_MIN_MDB;
  caps->gain_max_mdb = GAIN_MAX_MDB;
  caps->gain_step_mdb = GAIN_STEP_MDB;
  caps->gain_default_mdb = GAIN_MAX_MDB;
}

static int prv_dac_dma_start(const struct pbl_audio_sf32lb_audcodec *dev,
                             const struct pbl_audio_stream *stream) {
  size_t period_bytes = stream->period_frames * stream->channels * sizeof(int16_t);
  const struct pbl_dma_slave_config cfg = {
    .dst_addr = REG(DAC_CH0_ENTRY),
    .dst_addr_width = PBL_DMA_SLAVE_BUSWIDTH_4_BYTES,
  };
  int ret;

  dev->state->dac_ring_bytes = period_bytes * stream->periods;
  pbl_dcache_flush_range(stream->buf, dev->state->dac_ring_bytes);

  ret = pbl_dma_slave_config(dev->dac_dma, &cfg);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_dma_prep_dma_cyclic(dev->dac_dma, stream->buf, dev->state->dac_ring_bytes, period_bytes,
                                PBL_DMA_MEM_TO_DEV, prv_dma_done,
                                (void *)&dev->dai[PBL_AUDIO_PLAYBACK]);
  if (ret != 0) {
    return ret;
  }

  prv_set(REG(DAC_CH0_CFG), AUDCODEC_DAC_CH_CFG_DMA_EN);

  return 0;
}

static void prv_dac_off(const struct pbl_audio_sf32lb_audcodec *dev) {
  prv_clear(REG(DAC_CH0_CFG), AUDCODEC_DAC_CH_CFG_DMA_EN);

  prv_bypass(dev, true);
  prv_dac_analog_off(dev);
  prv_clear(REG(CFG), AUDCODEC_CFG_DAC_ENABLE);

  prv_clear(REG(DAC_CH0_CFG), AUDCODEC_DAC_CH_CFG_ENABLE);
  if (HAS_DAC2) {
    prv_clear(REG(DAC_CH1_CFG), AUDCODEC_DAC_CH_CFG_ENABLE);
  }
  prv_set(REG(DAC_CFG), AUDCODEC_DAC_CFG_PATH_RESET);
  prv_clear(REG(DAC_CFG), AUDCODEC_DAC_CFG_PATH_RESET);
}

static int prv_dac_powered_start(const struct pbl_audio_sf32lb_audcodec *dev,
                                 const struct pbl_audio_stream *stream, int32_t gain_mdb,
                                 bool mute) {
  const struct prv_rate *rate = prv_find_rate(stream->rate);
  int ret;

  ret = prv_clock_on(dev, rate);
  if (ret != 0) {
    return ret;
  }

  prv_dac_config(dev, rate);

  ret = prv_dac_dma_start(dev, stream);
  if (ret != 0) {
    return ret;
  }

  // The gain must be set before the DAC path opens, or startup transients play at 0 dB
  prv_apply(dev, gain_mdb, mute);

  prv_set(REG(CFG), AUDCODEC_CFG_DAC_ENABLE);
  prv_bypass(dev, true);
  prv_dac_analog_on(dev, rate);
  if (!mute) {
    prv_bypass(dev, false);
  }

  // The gain ramp keeps volume writes from latching
  prv_clear(REG(DAC_CH0_CFG_EXT), AUDCODEC_DAC_CH_CFG_EXT_RAMP_EN);
  if (HAS_DAC2) {
    prv_clear(REG(DAC_CH1_CFG_EXT), AUDCODEC_DAC_CH_CFG_EXT_RAMP_EN);
  }

  return 0;
}

static int prv_dac_start(const struct pbl_audio_dai *dai, const struct pbl_audio_stream *stream,
                         int32_t gain_mdb, bool mute) {
  const struct pbl_audio_sf32lb_audcodec *dev = prv_dev(dai);
  struct pbl_audio_sf32lb_audcodec_state *state = dev->state;
  int ret;

  if ((stream->period_frames * stream->channels * sizeof(int16_t)) % sizeof(uint32_t) != 0U) {
    return -EINVAL;
  }

  pbl_idle_state_block(SOC_SF32LB_IDLE_DEEPWFI);
  (void)pbl_mutex_lock(&state->lock, PBL_FOREVER);

  ret = prv_dac_powered_start(dev, stream, gain_mdb, mute);
  if (ret == 0) {
    state->running |= PBL_BIT(PBL_AUDIO_PLAYBACK);
  } else {
    (void)pbl_dma_terminate(dev->dac_dma);
    prv_dac_off(dev);
    prv_clock_put(dev);
  }

  pbl_mutex_unlock(&state->lock);
  if (ret != 0) {
    pbl_idle_state_release(SOC_SF32LB_IDLE_DEEPWFI);
  }

  return ret;
}

static int prv_dac_stop(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_sf32lb_audcodec *dev = prv_dev(dai);
  struct pbl_audio_sf32lb_audcodec_state *state = dev->state;
  int ret;

  (void)pbl_mutex_lock(&state->lock, PBL_FOREVER);

  ret = pbl_dma_terminate(dev->dac_dma);
  prv_dac_off(dev);

  state->running &= ~PBL_BIT(PBL_AUDIO_PLAYBACK);
  prv_clock_put(dev);

  pbl_mutex_unlock(&state->lock);
  pbl_idle_state_release(SOC_SF32LB_IDLE_DEEPWFI);

  return ret;
}

static int prv_dac_set_gain(const struct pbl_audio_dai *dai, int32_t gain_mdb) {
  prv_set_volume(prv_dev(dai), gain_mdb);

  return 0;
}

static int prv_dac_set_mute(const struct pbl_audio_dai *dai, bool mute) {
  prv_apply(prv_dev(dai), pbl_audio_dai_card(dai)->state->gain_mdb, mute);

  return 0;
}

static void prv_dac_sync_for_device(const struct pbl_audio_dai *dai, void *buf, size_t len) {
  pbl_dcache_flush_range(buf, len);
}

static int prv_dac_pointer(const struct pbl_audio_dai *dai, size_t *frames) {
  const struct pbl_audio_sf32lb_audcodec *dev = prv_dev(dai);

  return prv_pointer(dai, dev->dac_dma, dev->state->dac_ring_bytes, frames);
}

static int prv_init(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_sf32lb_audcodec *dev = prv_dev(dai);
  struct pbl_audio_sf32lb_audcodec_state *state = dev->state;
  int ret = 0;

  (void)pbl_mutex_lock(&state->lock, PBL_FOREVER);

  if (state->ready) {
    goto out;
  }

  ret = pbl_device_init(&dev->clk.ctrl->dev);
  if (ret != 0) {
    goto out;
  }

  ret = pbl_device_init(&dev->dac_dma->ctrl->dev);
  for (uint8_t ch = 0U; ch < ADC_CHANNELS_MAX && ret == 0; ch++) {
    if (dev->adc_dma[ch] != nullptr) {
      ret = pbl_device_init(&dev->adc_dma[ch]->ctrl->dev);
    }
  }
  if (ret != 0) {
    goto out;
  }

  ret = pbl_clk_enable(&dev->clk);
  if (ret != 0) {
    goto out;
  }

  pbl_modifyl(0U, PMUC_HXT_CR1_BUF_AUD_EN, dev->pmuc + PMUC_HXT_CR1_OFFSET);

  prv_dac_config(dev, prv_find_rate(INIT_RATE));

  ret = prv_calibrate(dev);
  if (ret != 0) {
    PBL_LOG_ERR("PLL calibration failed (%d)", ret);
    goto out;
  }

  state->ready = true;

out:
  pbl_mutex_unlock(&state->lock);

  return ret;
}

const struct pbl_audio_dai_ops pbl_audio_sf32lb_audcodec_dac_ops = {
  .init = prv_init,
  .get_caps = prv_dac_get_caps,
  .start = prv_dac_start,
  .stop = prv_dac_stop,
  .set_gain = prv_dac_set_gain,
  .set_mute = prv_dac_set_mute,
  .sync_for_device = prv_dac_sync_for_device,
  .pointer = prv_dac_pointer,
};

/* ADC */

static uintptr_t prv_adc_cfg1(const struct pbl_audio_sf32lb_audcodec *dev, uint8_t adc) {
  return adc == 0U ? AREG(ADC1_CFG1) : AREG(ADC2_CFG1);
}

static uintptr_t prv_adc_cfg2(const struct pbl_audio_sf32lb_audcodec *dev, uint8_t adc) {
  return adc == 0U ? AREG(ADC1_CFG2) : AREG(ADC2_CFG2);
}

static uintptr_t prv_adc_ch_cfg(const struct pbl_audio_sf32lb_audcodec *dev, uint8_t ch) {
  return ch == 0U ? REG(ADC_CH0_CFG) : REG(ADC_CH1_CFG);
}

static uintptr_t prv_adc_entry(const struct pbl_audio_sf32lb_audcodec *dev, uint8_t ch) {
  return ch == 0U ? REG(ADC_CH0_ENTRY) : REG(ADC_CH1_ENTRY);
}

// Gain is -60 dB plus rough steps of 6 dB and fine steps of 0.5 dB
static void prv_adc_set_volume(const struct pbl_audio_sf32lb_audcodec *dev, int32_t gain_mdb) {
  uint32_t steps = (uint32_t)pbl_audio_gain_to_step(gain_mdb, ADC_GAIN_MIN_MDB, GAIN_STEP_MDB);

  for (uint8_t ch = 0U; ch < dev->state->adc_channels; ch++) {
    pbl_writel((prv_read(prv_adc_ch_cfg(dev, ch)) &
                ~(AUDCODEC_ADC_CH_CFG_ROUGH_VOL_MASK | AUDCODEC_ADC_CH_CFG_FINE_VOL_MASK)) |
                   PBL_FIELD_PREP(AUDCODEC_ADC_CH_CFG_ROUGH_VOL_MASK, steps / FINE_STEPS) |
                   PBL_FIELD_PREP(AUDCODEC_ADC_CH_CFG_FINE_VOL_MASK, steps % FINE_STEPS),
               prv_adc_ch_cfg(dev, ch));
  }
}

static void prv_adc_config(const struct pbl_audio_sf32lb_audcodec *dev, const struct prv_rate *rate,
                           int32_t gain_mdb) {
  prv_write(REG(ADC_CFG), PBL_FIELD_PREP(AUDCODEC_ADC_CFG_OSR_SEL_MASK, rate->adc_osr_sel) |
                              PBL_FIELD_PREP(AUDCODEC_ADC_CFG_OP_MODE_MASK, ADC_OP_MODE) |
                              (rate->pll ? AUDCODEC_ADC_CFG_CLK_SRC_SEL : 0U) |
                              PBL_FIELD_PREP(AUDCODEC_ADC_CFG_CLK_DIV_MASK, rate->adc_clk_div));

  for (uint8_t ch = 0U; ch < dev->state->adc_channels; ch++) {
    prv_write(prv_adc_ch_cfg(dev, ch),
              AUDCODEC_ADC_CH_CFG_ENABLE |
                  PBL_FIELD_PREP(AUDCODEC_ADC_CH_CFG_HPF_COEF_MASK, ADC_HPF_COEF) |
                  AUDCODEC_ADC_CH_CFG_DATA_FORMAT);
  }

  prv_adc_set_volume(dev, gain_mdb);
}

static void prv_adc_analog_on(const struct pbl_audio_sf32lb_audcodec *dev,
                              const struct prv_rate *rate) {
  uint8_t adcs = dev->state->adc_channels;

#ifdef CONFIG_SOC_SF32LB57
  prv_set(AREG(ADC_ANA_CFG), SF32LB57_AUDCODEC_ADC_ANA_CFG_PSW_EN);
#endif
  prv_clear(REG(BG_CFG0), AUDCODEC_BG_CFG0_EN_SMPL);
  prv_set(AREG(ADC_ANA_CFG), AUDCODEC_ADC_ANA_CFG_MICBIAS_EN);
#ifndef CONFIG_SOC_SF32LB57
  prv_clear(AREG(ADC_ANA_CFG), AUDCODEC_ADC_ANA_CFG_MICBIAS_CHOP_EN);
#endif
  pbl_sleep_ms(MICBIAS_SETTLE_MS);

  prv_write(REG(PLL_CFG6),
            PBL_FIELD_PREP(AUDCODEC_PLL_CFG6_SEL_CLK_CHOP_MICBIAS_MASK, MICBIAS_SEL_CLK) |
                AUDCODEC_PLL_CFG6_EN_CLK_CHOP_MICBIAS |
                (rate->sel_clk_adc ? AUDCODEC_PLL_CFG6_SEL_CLK_ADC2 : 0U) |
                PBL_FIELD_PREP(AUDCODEC_PLL_CFG6_DIVA_CLK_ADC2_MASK, ADC_DIVA_CLK) |
                AUDCODEC_PLL_CFG6_EN_CLK_ADC2 |
                (rate->sel_clk_adc ? AUDCODEC_PLL_CFG6_SEL_CLK_ADC1 : 0U) |
                PBL_FIELD_PREP(AUDCODEC_PLL_CFG6_DIVA_CLK_ADC1_MASK, ADC_DIVA_CLK) |
                AUDCODEC_PLL_CFG6_EN_CLK_ADC1 |
                PBL_FIELD_PREP(AUDCODEC_PLL_CFG6_SEL_CLK_ADC0, ADC0_SEL_CLK) |
                PBL_FIELD_PREP(AUDCODEC_PLL_CFG6_DIVA_CLK_ADC0_MASK, ADC0_DIVA_CLK) |
                AUDCODEC_PLL_CFG6_EN_CLK_ADC0 |
                (rate->pll ? AUDCODEC_PLL_CFG6_SEL_CLK_ADC_SOURCE : 0U));

  prv_clock_reset(dev, PBL_AUDIO_CAPTURE, ADC_RESET_MS * 1000U);

#ifndef CONFIG_SOC_SF32LB57
  prv_clear(AREG(ADC1_CFG1), AUDCODEC_ADCN_CFG1_DACN_EN);
#endif
  for (uint8_t adc = 0U; adc < adcs; adc++) {
    // Single-ended inputs
    prv_clear(prv_adc_cfg1(dev, adc), AUDCODEC_ADCN_CFG1_DIFF_EN);
    prv_field(prv_adc_cfg1(dev, adc), AUDCODEC_ADCN_CFG1_FSP_MASK, rate->adc_fsp);
    prv_set(prv_adc_cfg1(dev, adc), ADCN_CFG1_VCMST);
    prv_set(prv_adc_cfg2(dev, adc), AUDCODEC_ADCN_CFG2_CLEAR);
    prv_field(prv_adc_cfg1(dev, adc), AUDCODEC_ADCN_CFG1_GC_MASK, ADC_GC);
    prv_set(prv_adc_cfg2(dev, adc), AUDCODEC_ADCN_CFG2_EN);
    prv_clear(prv_adc_cfg2(dev, adc), AUDCODEC_ADCN_CFG2_RSTB);
#ifndef CONFIG_SOC_SF32LB57
    prv_field(prv_adc_cfg1(dev, adc), AUDCODEC_ADCN_CFG1_VREF_SEL_MASK, ADC_VREF);
#endif
  }

  // The input common mode settles before the modulators leave reset
  pbl_sleep_ms(ADC_VCM_SETTLE_MS);

  for (uint8_t adc = 0U; adc < adcs; adc++) {
    prv_set(prv_adc_cfg2(dev, adc), AUDCODEC_ADCN_CFG2_RSTB);
    prv_clear(prv_adc_cfg1(dev, adc), ADCN_CFG1_VCMST);
    prv_clear(prv_adc_cfg2(dev, adc), AUDCODEC_ADCN_CFG2_CLEAR);
  }
}

static void prv_adc_analog_off(const struct pbl_audio_sf32lb_audcodec *dev) {
  prv_clear(AREG(ADC1_CFG2), AUDCODEC_ADCN_CFG2_EN);
  prv_clear(AREG(ADC2_CFG2), AUDCODEC_ADCN_CFG2_EN);
  prv_clear(AREG(ADC_ANA_CFG), AUDCODEC_ADC_ANA_CFG_MICBIAS_EN);
#ifdef CONFIG_SOC_SF32LB57
  prv_clear(AREG(ADC_ANA_CFG), SF32LB57_AUDCODEC_ADC_ANA_CFG_PSW_EN);
#endif
}

static void prv_adc_copy(const struct pbl_audio_sf32lb_audcodec *dev, uint32_t period) {
  const struct pbl_audio *card = pbl_audio_dai_card(&dev->dai[PBL_AUDIO_CAPTURE]);
  const struct pbl_audio_stream *stream = &card->state->stream;
  const size_t frames = stream->period_frames;
  int16_t *left = dev->state->adc_buf + period * frames;
  int16_t *right = left + frames * stream->periods;
  int16_t *out = pbl_audio_period(card, (uint8_t)period);

  pbl_dcache_invalidate_range(left, frames * sizeof(int16_t));
  pbl_dcache_invalidate_range(right, frames * sizeof(int16_t));

  for (size_t i = 0U; i < frames; i++) {
    out[2U * i] = left[i];
    out[2U * i + 1U] = right[i];
  }
}

// Each ADC fills its own ring; a period is interleaved once both have finished it
static void prv_adc_stereo_done(const struct pbl_audio_sf32lb_audcodec *dev, uint8_t ch,
                                int status) {
  struct pbl_audio_sf32lb_audcodec_state *state = dev->state;
  const struct pbl_audio *card = pbl_audio_dai_card(&dev->dai[PBL_AUDIO_CAPTURE]);
  uint32_t periods = card->state->stream.periods;
  uint32_t first;
  uint32_t last;

  if (status != 0) {
    pbl_audio_report_xrun(card);
    return;
  }

  pbl_irq_lock();
  state->adc_done[ch]++;
  first = state->adc_copied;
  last = PBL_MIN(state->adc_done[0], state->adc_done[1]);
  state->adc_copied = last;
  pbl_irq_unlock();

  for (uint32_t period = first; period != last; period++) {
    prv_adc_copy(dev, period % periods);
    pbl_audio_period_elapsed(card);
  }
}

static void prv_adc1_done(void *arg, int status) {
  prv_adc_stereo_done(arg, 0U, status);
}

static void prv_adc2_done(void *arg, int status) {
  prv_adc_stereo_done(arg, 1U, status);
}

static int prv_adc_dma_start(const struct pbl_audio_sf32lb_audcodec *dev,
                             const struct pbl_audio_stream *stream) {
  struct pbl_audio_sf32lb_audcodec_state *state = dev->state;
  const size_t period_bytes = stream->period_frames * sizeof(int16_t);
  const size_t ring_bytes = period_bytes * stream->periods;
  int ret;

  if (stream->channels == 1U) {
    state->adc_ring_bytes = ring_bytes;
    pbl_dcache_flush_invalidate_range(stream->buf, ring_bytes);
  } else {
    state->adc_buf = pbl_audio_buf_alloc(PBL_DCACHE_ROUND_UP(ring_bytes * stream->channels));
    if (state->adc_buf == nullptr) {
      return -ENOMEM;
    }
    pbl_dcache_flush_invalidate_range(state->adc_buf, ring_bytes * stream->channels);
    state->adc_done[0] = 0U;
    state->adc_done[1] = 0U;
    state->adc_copied = 0U;
  }

  for (uint8_t ch = 0U; ch < stream->channels; ch++) {
    const struct pbl_dma_slave_config cfg = {
      .src_addr = prv_adc_entry(dev, ch),
      .src_addr_width = PBL_DMA_SLAVE_BUSWIDTH_4_BYTES,
    };

    ret = pbl_dma_slave_config(dev->adc_dma[ch], &cfg);
    if (ret != 0) {
      return ret;
    }

    if (stream->channels == 1U) {
      ret = pbl_dma_prep_dma_cyclic(dev->adc_dma[ch], stream->buf, ring_bytes, period_bytes,
                                    PBL_DMA_DEV_TO_MEM, prv_dma_done,
                                    (void *)&dev->dai[PBL_AUDIO_CAPTURE]);
    } else {
      ret = pbl_dma_prep_dma_cyclic(dev->adc_dma[ch], (uint8_t *)state->adc_buf + ch * ring_bytes,
                                    ring_bytes, period_bytes, PBL_DMA_DEV_TO_MEM,
                                    ch == 0U ? prv_adc1_done : prv_adc2_done, (void *)dev);
    }
    if (ret != 0) {
      return ret;
    }

    prv_set(prv_adc_ch_cfg(dev, ch), AUDCODEC_ADC_CH_CFG_DMA_EN);
  }

  return 0;
}

static void prv_adc_off(const struct pbl_audio_sf32lb_audcodec *dev) {
  struct pbl_audio_sf32lb_audcodec_state *state = dev->state;

  for (uint8_t ch = 0U; ch < ADC_CHANNELS_MAX; ch++) {
    if (dev->adc_dma[ch] != nullptr) {
      (void)pbl_dma_terminate(dev->adc_dma[ch]);
    }
  }

  prv_clear(REG(CFG), AUDCODEC_CFG_ADC_ENABLE);
  prv_clear(REG(ADC_CH0_CFG), AUDCODEC_ADC_CH_CFG_ENABLE | AUDCODEC_ADC_CH_CFG_DMA_EN);
  prv_clear(REG(ADC_CH1_CFG), AUDCODEC_ADC_CH_CFG_ENABLE | AUDCODEC_ADC_CH_CFG_DMA_EN);
  prv_set(REG(ADC_CFG), AUDCODEC_ADC_CFG_PATH_RESET);
  prv_clear(REG(ADC_CFG), AUDCODEC_ADC_CFG_PATH_RESET);
  prv_adc_analog_off(dev);

  pbl_audio_buf_free(state->adc_buf);
  state->adc_buf = nullptr;
}

static void prv_adc_get_caps(const struct pbl_audio_dai *dai, struct pbl_audio_caps *caps) {
  const struct pbl_audio_sf32lb_audcodec *dev = prv_dev(dai);

  prv_get_rates(caps);
  caps->min_channels = 1U;
  caps->max_channels = (ADC_CHANNELS_MAX > 1U && dev->adc_dma[1] != nullptr) ? 2U : 1U;
  caps->max_periods = 2U;
  caps->gain_min_mdb = ADC_GAIN_MIN_MDB;
  caps->gain_max_mdb = ADC_GAIN_MAX_MDB;
  caps->gain_step_mdb = GAIN_STEP_MDB;
  caps->gain_default_mdb = ADC_GAIN_DEFAULT_MDB;
}

static int prv_adc_powered_start(const struct pbl_audio_sf32lb_audcodec *dev,
                                 const struct pbl_audio_stream *stream, int32_t gain_mdb) {
  const struct prv_rate *rate = prv_find_rate(stream->rate);
  int ret;

  ret = pbl_pinctrl_select_state(&dev->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0 && ret != -ENOENT) {
    return ret;
  }

  ret = prv_clock_on(dev, rate);
  if (ret != 0) {
    return ret;
  }

  prv_adc_config(dev, rate, gain_mdb);

  ret = prv_adc_dma_start(dev, stream);
  if (ret != 0) {
    return ret;
  }

  prv_adc_analog_on(dev, rate);
  prv_set(REG(CFG), AUDCODEC_CFG_ADC_ENABLE);

  return 0;
}

static int prv_adc_start(const struct pbl_audio_dai *dai, const struct pbl_audio_stream *stream,
                         int32_t gain_mdb, bool mute) {
  const struct pbl_audio_sf32lb_audcodec *dev = prv_dev(dai);
  struct pbl_audio_sf32lb_audcodec_state *state = dev->state;
  int ret;

  if (stream->channels > ADC_CHANNELS_MAX || dev->adc_dma[stream->channels - 1U] == nullptr ||
      (stream->period_frames * sizeof(int16_t)) % sizeof(uint32_t) != 0U) {
    return -EINVAL;
  }

  pbl_idle_state_block(SOC_SF32LB_IDLE_DEEPWFI);
  (void)pbl_mutex_lock(&state->lock, PBL_FOREVER);

  state->adc_channels = stream->channels;
  ret = prv_adc_powered_start(dev, stream, mute ? ADC_GAIN_MIN_MDB : gain_mdb);
  if (ret == 0) {
    state->running |= PBL_BIT(PBL_AUDIO_CAPTURE);
  } else {
    prv_adc_off(dev);
    prv_clock_put(dev);
  }

  pbl_mutex_unlock(&state->lock);
  if (ret != 0) {
    pbl_idle_state_release(SOC_SF32LB_IDLE_DEEPWFI);
  }

  return ret;
}

static int prv_adc_stop(const struct pbl_audio_dai *dai) {
  const struct pbl_audio_sf32lb_audcodec *dev = prv_dev(dai);
  struct pbl_audio_sf32lb_audcodec_state *state = dev->state;

  (void)pbl_mutex_lock(&state->lock, PBL_FOREVER);

  prv_adc_off(dev);
  state->running &= ~PBL_BIT(PBL_AUDIO_CAPTURE);
  prv_clock_put(dev);

  pbl_mutex_unlock(&state->lock);
  pbl_idle_state_release(SOC_SF32LB_IDLE_DEEPWFI);

  return 0;
}

static int prv_adc_set_gain(const struct pbl_audio_dai *dai, int32_t gain_mdb) {
  prv_adc_set_volume(prv_dev(dai), gain_mdb);

  return 0;
}

// Stereo periods are written by the CPU
static void prv_adc_sync_for_cpu(const struct pbl_audio_dai *dai, void *buf, size_t len) {
  if (prv_dev(dai)->state->adc_channels == 1U) {
    pbl_dcache_invalidate_range(buf, len);
  }
}

static int prv_adc_pointer(const struct pbl_audio_dai *dai, size_t *frames) {
  const struct pbl_audio_sf32lb_audcodec *dev = prv_dev(dai);

  if (dev->state->adc_channels != 1U) {
    return -ENOTSUP;
  }

  return prv_pointer(dai, dev->adc_dma[0], dev->state->adc_ring_bytes, frames);
}

const struct pbl_audio_dai_ops pbl_audio_sf32lb_audcodec_adc_ops = {
  .init = prv_init,
  .get_caps = prv_adc_get_caps,
  .start = prv_adc_start,
  .stop = prv_adc_stop,
  .set_gain = prv_adc_set_gain,
  .sync_for_cpu = prv_adc_sync_for_cpu,
  .pointer = prv_adc_pointer,
};

#include PBL_DT_INSTANCES(sifli_sf32lb52_audcodec)
#include PBL_DT_INSTANCES(sifli_sf32lb57_audcodec)
