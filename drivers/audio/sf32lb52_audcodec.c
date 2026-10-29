/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/audio/sf32lb52.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/dma/sf32lb52.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/cache.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/sf32lb52_audcodec.h>
#include <hardware/sf32lb52_pmuc.h>

PBL_LOG_MODULE_DEFINE(driver_audio_sf32lb52, CONFIG_DRIVER_AUDIO_LOG_LEVEL);

#define REG(name) (SF32LB52_AUDCODEC_BASE + SF32LB52_AUDCODEC_##name##_OFFSET)

#define GAIN_MIN_MDB  (-36000)
#define GAIN_MAX_MDB  0
#define GAIN_STEP_MDB 500
// Fine volume steps per rough volume step
#define FINE_STEPS    12U
#define ROUGH_VOL_0DB ((GAIN_MAX_MDB - GAIN_MIN_MDB) / GAIN_STEP_MDB / FINE_STEPS)
// Fine volume of a muted output
#define FINE_VOL_MUTE 0xfU

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

#define PLL_ICP_SEL 8U
#define PLL_R3_SEL  3U
#define PLL_RZ_SEL  1U
#define PLL_C2_SEL  3U
#define PLL_CZ_SEL  6U
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
};

static const struct prv_rate s_rates[] = {
  {48000U, false, 1U, 0U, false}, {32000U, false, 1U, 1U, false}, {24000U, false, 1U, 5U, true},
  {16000U, false, 1U, 4U, false}, {12000U, false, 1U, 7U, true},  {8000U, false, 1U, 8U, true},
  {44100U, true, 1U, 0U, false},  {22050U, true, 1U, 5U, true},   {11025U, true, 1U, 7U, true},
};

static const struct pbl_clk s_clk = PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_AUDCODEC);

static const struct pbl_audio_sf32lb52_audcodec *prv_dev(const struct pbl_audio *audio) {
  return container_of(audio, const struct pbl_audio_sf32lb52_audcodec, audio);
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
  for (size_t i = 0U; i < ARRAY_LENGTH(s_rates); i++) {
    if (s_rates[i].rate == rate) {
      return &s_rates[i];
    }
  }

  return nullptr;
}

static void prv_refgen_on(void) {
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
static void prv_bias_on(void) {
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
  prv_set(REG(PLL_CFG4), AUDCODEC_PLL_CFG4_EN_CLK_DIG);
  prv_write(REG(PLL_CFG1), PBL_FIELD_PREP(AUDCODEC_PLL_CFG1_R3_SEL_MASK, PLL_R3_SEL) |
                               PBL_FIELD_PREP(AUDCODEC_PLL_CFG1_RZ_SEL_MASK, PLL_RZ_SEL) |
                               PBL_FIELD_PREP(AUDCODEC_PLL_CFG1_C2_SEL_MASK, PLL_C2_SEL) |
                               PBL_FIELD_PREP(AUDCODEC_PLL_CFG1_CZ_SEL_MASK, PLL_CZ_SEL));
  pbl_busy_wait_us(PLL_SETTLE_US);

  prv_refgen_on();
}

static void prv_bias_off(void) {
  prv_clear(REG(PLL_CFG0),
            AUDCODEC_PLL_CFG0_EN_IARY | AUDCODEC_PLL_CFG0_EN_VCO | AUDCODEC_PLL_CFG0_EN_ANA);
  prv_clear(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_EN_DIG);
  prv_clear(REG(PLL_CFG3), AUDCODEC_PLL_CFG3_EN_SDM);
  prv_clear(REG(PLL_CFG4), AUDCODEC_PLL_CFG4_EN_CLK_DIG);
  prv_clear(REG(REFGEN_CFG), AUDCODEC_REFGEN_CFG_EN);
  prv_write(REG(BG_CFG1), 0U);
  prv_write(REG(BG_CFG2), 0U);
  prv_clear(REG(BG_CFG0), AUDCODEC_BG_CFG0_EN);
  prv_clear(REG(BG_CFG0), AUDCODEC_BG_CFG0_EN_SMPL);
}

static int prv_cal_count(uint32_t fc_vco, uint32_t *count) {
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

static int prv_cal_vco(uint32_t target, uint32_t *deltas, uint32_t *vco) {
  uint32_t fc_vco = FC_VCO_START;
  uint32_t fc_min;
  uint32_t fc_max;
  uint32_t count;
  int ret;

  for (uint32_t step = FC_VCO_STEP; step != 0U; step >>= 1) {
    ret = prv_cal_count(fc_vco, &count);
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

  ret = prv_cal_count(fc_min, &count);
  if (ret != 0) {
    return ret;
  }
  prv_update_delta(count, target, &deltas[1]);

  ret = prv_cal_count(fc_max, &count);
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
static int prv_calibrate(const struct pbl_audio_sf32lb52_audcodec *dev) {
  uint32_t deltas[3] = {0U};
  int ret = 0;

  prv_bias_on();

  prv_set(REG(PLL_CFG0), AUDCODEC_PLL_CFG0_OPEN);
  prv_set(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_EN_LF_VCIN);
  prv_write(REG(PLL_CAL_CFG), PBL_FIELD_PREP(AUDCODEC_PLL_CAL_CFG_LEN_MASK, CAL_LEN));

  for (size_t i = 0U; i < ARRAY_LENGTH(s_vco_target) && ret == 0; i++) {
    ret = prv_cal_vco(s_vco_target[i], deltas, &dev->state->vco[i]);
  }

  prv_clear(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_EN_LF_VCIN);
  prv_clear(REG(PLL_CFG0), AUDCODEC_PLL_CFG0_OPEN);

  prv_bias_off();

  return ret;
}

static int prv_pll_lock_44m1(void) {
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

static int prv_clock_on(const struct pbl_audio_sf32lb52_audcodec *dev,
                        const struct prv_rate *rate) {
  struct pbl_audio_sf32lb52_audcodec_state *state = dev->state;
  int ret = -EIO;

  if (state->pll == PLL_OFF) {
    prv_bias_on();
    state->pll = PLL_BIAS;
  }

  if (!rate->pll) {
    return 0;
  }

  prv_field(REG(PLL_CFG0), AUDCODEC_PLL_CFG0_FC_VCO_MASK, state->vco[VCO_44M1]);
  for (uint32_t i = 0U; i < PLL_LOCK_TRIES && ret != 0; i++) {
    ret = prv_pll_lock_44m1();
  }
  if (ret != 0) {
    PBL_LOG_ERR("PLL did not lock");
    return ret;
  }

  state->pll = PLL_LOCKED;

  return 0;
}

static void prv_clock_off(const struct pbl_audio_sf32lb52_audcodec *dev) {
  prv_bias_off();
  dev->state->pll = PLL_OFF;
}

static void prv_config_channel(const struct prv_rate *rate) {
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

static void prv_bypass(bool bypass) {
  uint32_t debug = PBL_FIELD_PREP(AUDCODEC_DAC_CH_DEBUG_DATA_OUT_MASK, DEBUG_DATA_OUT) |
                   (bypass ? AUDCODEC_DAC_CH_DEBUG_BYPASS : 0U);

  if (bypass) {
    prv_set(REG(DAC_CH0_CFG), AUDCODEC_DAC_CH_CFG_DOUT_MUTE);
    prv_write(REG(DAC_CH0_DEBUG), debug);
    prv_set(REG(DAC_CH1_CFG), AUDCODEC_DAC_CH_CFG_DOUT_MUTE);
    prv_write(REG(DAC_CH1_DEBUG), debug);
  } else {
    prv_clear(REG(DAC_CH0_CFG), AUDCODEC_DAC_CH_CFG_DOUT_MUTE);
    prv_write(REG(DAC_CH0_DEBUG), debug);
    prv_clear(REG(DAC_CH1_CFG), AUDCODEC_DAC_CH_CFG_DOUT_MUTE);
    prv_write(REG(DAC_CH1_DEBUG), debug);
  }
}

// Gain is -36 dB plus rough steps of 6 dB and fine steps of 0.5 dB
static void prv_set_volume(int32_t gain_mdb) {
  uint32_t steps = (uint32_t)pbl_audio_gain_to_step(gain_mdb, GAIN_MIN_MDB, GAIN_STEP_MDB);
  uint32_t mask = AUDCODEC_DAC_CH_CFG_ROUGH_VOL_MASK | AUDCODEC_DAC_CH_CFG_FINE_VOL_MASK;
  uint32_t value = PBL_FIELD_PREP(AUDCODEC_DAC_CH_CFG_ROUGH_VOL_MASK, steps / FINE_STEPS) |
                   PBL_FIELD_PREP(AUDCODEC_DAC_CH_CFG_FINE_VOL_MASK, steps % FINE_STEPS);

  pbl_writel((prv_read(REG(DAC_CH0_CFG)) & ~mask) | value, REG(DAC_CH0_CFG));
  pbl_writel((prv_read(REG(DAC_CH1_CFG)) & ~mask) | value, REG(DAC_CH1_CFG));
}

static void prv_apply(int32_t gain_mdb, bool mute) {
  if (mute) {
    prv_bypass(true);
    prv_field(REG(DAC_CH0_CFG), AUDCODEC_DAC_CH_CFG_FINE_VOL_MASK, FINE_VOL_MUTE);
    prv_field(REG(DAC_CH1_CFG), AUDCODEC_DAC_CH_CFG_FINE_VOL_MASK, FINE_VOL_MUTE);
    return;
  }

  prv_bypass(false);
  prv_set_volume(gain_mdb);
}

static void prv_analog_on(const struct prv_rate *rate) {
  prv_clear(REG(PLL_CFG4), AUDCODEC_PLL_CFG4_SEL_CLK_DAC | AUDCODEC_PLL_CFG4_SEL_CLK_DAC_SOURCE);
  prv_set(REG(PLL_CFG4), AUDCODEC_PLL_CFG4_EN_CLK_CHOP_DAC | AUDCODEC_PLL_CFG4_EN_CLK_DAC |
                             (rate->pll ? AUDCODEC_PLL_CFG4_SEL_CLK_DAC_SOURCE : 0U) |
                             (rate->sel_clk_dac ? AUDCODEC_PLL_CFG4_SEL_CLK_DAC : 0U) |
                             AUDCODEC_PLL_CFG4_EN_CLK_DIG);
  prv_set(REG(PLL_CFG5), AUDCODEC_PLL_CFG5_EN_CLK_CHOP_BG | AUDCODEC_PLL_CFG5_EN_CLK_CHOP_REFGEN);

  prv_clear(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_RSTB);
  pbl_busy_wait_us(PLL_RESET_US);
  prv_set(REG(PLL_CFG2), AUDCODEC_PLL_CFG2_RSTB);

  prv_clear(REG(DAC1_CFG), AUDCODEC_DACN_CFG_LP_MODE);
  prv_clear(REG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_OS_DAC);
  prv_clear(REG(DAC2_CFG), AUDCODEC_DACN_CFG_EN_OS_DAC);
  prv_set(REG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_VCM);
  prv_clear(REG(DAC2_CFG), AUDCODEC_DACN_CFG_EN_VCM);
  pbl_busy_wait_us(DAC_VCM_US);
  prv_set(REG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_AMP);
  prv_clear(REG(DAC2_CFG), AUDCODEC_DACN_CFG_EN_AMP);
  pbl_busy_wait_us(DAC_AMP_US);
  prv_set(REG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_OS_DAC);
  prv_set(REG(DAC2_CFG), AUDCODEC_DACN_CFG_EN_OS_DAC);
  pbl_busy_wait_us(DAC_STEP_US);
  prv_set(REG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_DAC);
  prv_clear(REG(DAC2_CFG), AUDCODEC_DACN_CFG_EN_DAC);
  pbl_busy_wait_us(DAC_STEP_US);
  prv_clear(REG(DAC1_CFG), AUDCODEC_DACN_CFG_SR);
  prv_clear(REG(DAC2_CFG), AUDCODEC_DACN_CFG_SR);
}

static void prv_analog_off(void) {
  prv_set(REG(DAC1_CFG), AUDCODEC_DACN_CFG_SR);
  prv_set(REG(DAC2_CFG), AUDCODEC_DACN_CFG_SR);
  pbl_busy_wait_us(DAC_STEP_US);
  prv_clear(REG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_DAC);
  prv_clear(REG(DAC2_CFG), AUDCODEC_DACN_CFG_EN_DAC);
  pbl_busy_wait_us(DAC_STEP_US);
  prv_clear(REG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_VCM);
  prv_clear(REG(DAC2_CFG), AUDCODEC_DACN_CFG_EN_VCM);
  pbl_busy_wait_us(DAC_STEP_US);
  prv_clear(REG(DAC1_CFG), AUDCODEC_DACN_CFG_EN_AMP | AUDCODEC_DACN_CFG_EN_OS_DAC);
  prv_clear(REG(DAC2_CFG), AUDCODEC_DACN_CFG_EN_AMP | AUDCODEC_DACN_CFG_EN_OS_DAC);
}

static void prv_dma_done(void *arg, int status) {
  const struct pbl_audio *audio = arg;

  if (status != 0) {
    pbl_audio_report_xrun(audio);
    return;
  }

  pbl_audio_period_elapsed(audio);
}

static int prv_init(const struct pbl_audio *audio) {
  const struct pbl_audio_sf32lb52_audcodec *dev = prv_dev(audio);
  int ret;

  if (dcache_line_size() > PBL_AUDIO_BUF_ALIGN) {
    return -EINVAL;
  }

  *dev->state = (struct pbl_audio_sf32lb52_audcodec_state){0};

  ret = pbl_device_init(&s_clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&s_clk);
  if (ret != 0) {
    return ret;
  }

  pbl_modifyl(0U, PMUC_HXT_CR1_BUF_AUD_EN, SF32LB52_PMUC_BASE + SF32LB52_PMUC_HXT_CR1_OFFSET);

  prv_config_channel(prv_find_rate(INIT_RATE));

  ret = prv_calibrate(dev);
  if (ret != 0) {
    PBL_LOG_ERR("PLL calibration failed (%d)", ret);
  }

  return ret;
}

static void prv_get_caps(const struct pbl_audio *audio, struct pbl_audio_caps *caps) {
  for (size_t i = 0U; i < ARRAY_LENGTH(s_rates); i++) {
    caps->rates |= pbl_audio_rate_bit(s_rates[i].rate);
  }
  caps->min_channels = 1U;
  caps->max_channels = 1U;
  caps->max_periods = 2U;
  caps->gain_min_mdb = GAIN_MIN_MDB;
  caps->gain_max_mdb = GAIN_MAX_MDB;
  caps->gain_step_mdb = GAIN_STEP_MDB;
  caps->gain_default_mdb = GAIN_MAX_MDB;
}

static int prv_dma_start(const struct pbl_audio_sf32lb52_audcodec *dev,
                         const struct pbl_audio_stream *stream) {
  size_t period_bytes = stream->period_frames * stream->channels * sizeof(int16_t);
  const struct pbl_dma_slave_config cfg = {
    .dst_addr = REG(DAC_CH0_ENTRY),
    .dst_addr_width = PBL_DMA_SLAVE_BUSWIDTH_4_BYTES,
  };
  int ret;

  dev->state->ring_bytes = period_bytes * stream->periods;
  dcache_flush(stream->buf, dev->state->ring_bytes);

  ret = pbl_dma_slave_config(dev->dma, &cfg);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_dma_prep_dma_cyclic(dev->dma, stream->buf, dev->state->ring_bytes, period_bytes,
                                PBL_DMA_MEM_TO_DEV, prv_dma_done, (void *)&dev->audio);
  if (ret != 0) {
    return ret;
  }

  prv_set(REG(DAC_CH0_CFG), AUDCODEC_DAC_CH_CFG_DMA_EN);
  pbl_irq_enable(dev->dma_irq);

  return 0;
}

static int prv_start(const struct pbl_audio *audio, const struct pbl_audio_stream *stream,
                     int32_t gain_mdb, bool mute) {
  const struct pbl_audio_sf32lb52_audcodec *dev = prv_dev(audio);
  const struct prv_rate *rate = prv_find_rate(stream->rate);
  int ret;

  if ((stream->period_frames * stream->channels * sizeof(int16_t)) % sizeof(uint32_t) != 0U) {
    return -EINVAL;
  }

  soc_sf32lb_sleep_block(SOC_SF32LB_DEEPWFI);

  ret = prv_clock_on(dev, rate);
  if (ret != 0) {
    prv_clock_off(dev);
    soc_sf32lb_sleep_release(SOC_SF32LB_DEEPWFI);
    return ret;
  }

  prv_config_channel(rate);

  ret = prv_dma_start(dev, stream);
  if (ret != 0) {
    prv_clock_off(dev);
    soc_sf32lb_sleep_release(SOC_SF32LB_DEEPWFI);
    return ret;
  }

  // The gain must be set before the DAC path opens, or startup transients play at 0 dB
  prv_apply(gain_mdb, mute);

  prv_set(REG(CFG), AUDCODEC_CFG_DAC_ENABLE);
  prv_bypass(true);
  prv_analog_on(rate);
  if (!mute) {
    prv_bypass(false);
  }

  // The gain ramp keeps volume writes from latching
  prv_clear(REG(DAC_CH0_CFG_EXT), AUDCODEC_DAC_CH_CFG_EXT_RAMP_EN);
  prv_clear(REG(DAC_CH1_CFG_EXT), AUDCODEC_DAC_CH_CFG_EXT_RAMP_EN);

  return 0;
}

static int prv_stop(const struct pbl_audio *audio) {
  const struct pbl_audio_sf32lb52_audcodec *dev = prv_dev(audio);
  int ret;

  prv_clock_off(dev);

  pbl_irq_disable(dev->dma_irq);
  ret = pbl_dma_terminate(dev->dma);
  prv_clear(REG(DAC_CH0_CFG), AUDCODEC_DAC_CH_CFG_DMA_EN);

  prv_bypass(true);
  prv_analog_off();
  prv_clear(REG(CFG), AUDCODEC_CFG_DAC_ENABLE);

  prv_clear(REG(DAC_CH0_CFG), AUDCODEC_DAC_CH_CFG_ENABLE);
  prv_clear(REG(DAC_CH1_CFG), AUDCODEC_DAC_CH_CFG_ENABLE);
  prv_set(REG(DAC_CFG), AUDCODEC_DAC_CFG_PATH_RESET);
  prv_clear(REG(DAC_CFG), AUDCODEC_DAC_CFG_PATH_RESET);

  soc_sf32lb_sleep_release(SOC_SF32LB_DEEPWFI);

  return ret;
}

static int prv_set_gain(const struct pbl_audio *audio, int32_t gain_mdb) {
  prv_set_volume(gain_mdb);

  return 0;
}

static int prv_set_mute(const struct pbl_audio *audio, bool mute) {
  prv_apply(audio->state->gain_mdb, mute);

  return 0;
}

static void prv_sync_for_device(const struct pbl_audio *audio, void *buf, size_t len) {
  dcache_flush(buf, len);
}

static int prv_pointer(const struct pbl_audio *audio, size_t *frames) {
  const struct pbl_audio_sf32lb52_audcodec *dev = prv_dev(audio);
  const struct pbl_audio_state *state = audio->state;
  size_t residue;
  int ret;

  ret = pbl_dma_tx_status(dev->dma, &residue);
  if (ret != 0) {
    return ret;
  }

  *frames = ((dev->state->ring_bytes - residue) % state->period_bytes) /
            (state->stream.channels * sizeof(int16_t));

  return 0;
}

const struct pbl_audio_ops pbl_audio_sf32lb52_audcodec_ops = {
  .init = prv_init,
  .get_caps = prv_get_caps,
  .start = prv_start,
  .stop = prv_stop,
  .set_gain = prv_set_gain,
  .set_mute = prv_set_mute,
  .sync_for_device = prv_sync_for_device,
  .pointer = prv_pointer,
};
