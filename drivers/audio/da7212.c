/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/audio/da7212.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DEFINE(driver_audio_da7212, CONFIG_DRIVER_AUDIO_LOG_LEVEL);

#define DA7212_PLL_STATUS          0x03U
#define DA7212_CIF_CTRL            0x1dU
#define DA7212_DIG_ROUTING_DAI     0x21U
#define DA7212_SR                  0x22U
#define DA7212_REFERENCES          0x23U
#define DA7212_PLL_FRAC_TOP        0x24U
#define DA7212_PLL_FRAC_BOT        0x25U
#define DA7212_PLL_INTEGER         0x26U
#define DA7212_PLL_CTRL            0x27U
#define DA7212_DAI_CLK_MODE        0x28U
#define DA7212_DAI_CTRL            0x29U
#define DA7212_DIG_ROUTING_DAC     0x2aU
#define DA7212_DAC_FILTERS5        0x40U
#define DA7212_DAC_R_GAIN          0x46U
#define DA7212_LINE_GAIN           0x4aU
#define DA7212_MIXOUT_R_SELECT     0x4cU
#define DA7212_SYSTEM_MODES_OUTPUT 0x51U
#define DA7212_DAC_R_CTRL          0x6aU
#define DA7212_LINE_CTRL           0x6dU
#define DA7212_MIXOUT_R_CTRL       0x6fU
#define DA7212_LDO_CTRL            0x90U
#define DA7212_GAIN_RAMP_CTRL      0x92U
#define DA7212_TEST_UNLOCK         0xf0U
#define DA7212_TEST_PLL            0xf2U
#define DA7212_SYSTEM_ACTIVE       0xfdU

#define DA7212_CIF_CTRL_SOFT_RESET        0x80U
#define DA7212_SYSTEM_ACTIVE_ON           0x01U
#define DA7212_REFERENCES_BIAS_EN         0x08U
#define DA7212_LDO_CTRL_EN                0x80U
#define DA7212_PLL_STATUS_LOCKED          0x07U
#define DA7212_SR_16KHZ                   0x05U
#define DA7212_DAC_FILTERS5_SOFTMUTE      0x80U
#define DA7212_DAI_CTRL_EN_16BIT          0x80U
#define DA7212_DAI_CLK_MODE_MASTER_64BCLK 0x81U

// DAC_R_GAIN: 0.75 dB steps, 0x6f is 0 dB
#define GAIN_CODE_MIN 0x07U
#define GAIN_CODE_0DB 0x6fU
#define GAIN_CODE_MAX 0x7fU
#define GAIN_STEP_MDB 750
#define GAIN_MIN_MDB  (((int32_t)GAIN_CODE_MIN - (int32_t)GAIN_CODE_0DB) * GAIN_STEP_MDB)
#define GAIN_MAX_MDB  (((int32_t)GAIN_CODE_MAX - (int32_t)GAIN_CODE_0DB) * GAIN_STEP_MDB)

#define RESET_MS      10U
#define REFERENCES_MS 30U
#define PLL_LOCK_MS   40U

#define RATE 16000U

struct prv_reg_seq {
  uint8_t reg;
  uint8_t val;
  uint8_t delay_ms;
};

// PLL from a 4 MHz master clock (input divider 2) to a 12.288 MHz system clock: VCO 98.304 MHz,
// integer 49, fraction 0x4dd. Then the 13.29 workaround of silicon 3.6 for 2-5 MHz master clocks.
static const struct prv_reg_seq s_power_up[] = {
  {DA7212_CIF_CTRL, DA7212_CIF_CTRL_SOFT_RESET, RESET_MS},
  {DA7212_SYSTEM_ACTIVE, DA7212_SYSTEM_ACTIVE_ON, 0U},
  {DA7212_REFERENCES, DA7212_REFERENCES_BIAS_EN, REFERENCES_MS},
  {DA7212_LDO_CTRL, DA7212_LDO_CTRL_EN, 0U},
  {DA7212_PLL_FRAC_TOP, 0x04U, 0U},
  {DA7212_PLL_FRAC_BOT, 0xddU, 0U},
  {DA7212_PLL_INTEGER, 0x31U, 0U},
  {DA7212_PLL_CTRL, 0xc0U, 0U},
  {DA7212_TEST_UNLOCK, 0x8bU, 0U},
  {DA7212_TEST_PLL, 0x03U, 0U},
  {DA7212_TEST_UNLOCK, 0x00U, PLL_LOCK_MS},
};

// DAI to the right DAC, mixer and line output; no gain ramp or soft mixing, which delay or ramp
// every change
static const struct prv_reg_seq s_path[] = {
  {DA7212_GAIN_RAMP_CTRL, 0x00U, 0U},  {DA7212_SR, DA7212_SR_16KHZ, 0U},
  {DA7212_DIG_ROUTING_DAI, 0x32U, 0U}, {DA7212_DIG_ROUTING_DAC, 0xbaU, 0U},
  {DA7212_DAC_R_CTRL, 0x80U, 0U},      {DA7212_MIXOUT_R_SELECT, 0x08U, 0U},
  {DA7212_MIXOUT_R_CTRL, 0x90U, 0U},   {DA7212_LINE_GAIN, 0x30U, 0U},
  {DA7212_LINE_CTRL, 0x80U, 0U},       {DA7212_SYSTEM_MODES_OUTPUT, 0x89U, 0U},
};

// The DAI starts as clock master, driving the bit and frame clocks
static const struct prv_reg_seq s_dai_master[] = {
  {DA7212_DAI_CTRL, DA7212_DAI_CTRL_EN_16BIT, 0U},
  {DA7212_DAI_CLK_MODE, DA7212_DAI_CLK_MODE_MASTER_64BCLK, 0U},
};

static const struct pbl_audio_da7212 *prv_dev(const struct pbl_audio_codec *codec) {
  return container_of(codec, const struct pbl_audio_da7212, codec);
}

static int prv_write(const struct pbl_audio_da7212 *dev, uint8_t reg, uint8_t val) {
  return pbl_i2c_reg_write_byte(&dev->i2c, reg, val);
}

static int prv_write_seq(const struct pbl_audio_da7212 *dev, const struct prv_reg_seq *seq,
                         size_t len) {
  int ret;

  for (size_t i = 0U; i < len; i++) {
    ret = prv_write(dev, seq[i].reg, seq[i].val);
    if (ret != 0) {
      return ret;
    }

    if (seq[i].delay_ms != 0U) {
      pbl_sleep_ms(seq[i].delay_ms);
    }
  }

  return 0;
}

int pbl_audio_da7212_init(const struct pbl_device *dev) {
  const struct pbl_audio_da7212 *da7212 =
      container_of(dev, const struct pbl_audio_da7212, codec.dev);

  return prv_write(da7212, DA7212_SYSTEM_ACTIVE, 0U);
}

static void prv_get_caps(const struct pbl_audio_codec *codec, struct pbl_audio_caps *caps) {
  caps->gain_min_mdb = GAIN_MIN_MDB;
  caps->gain_max_mdb = GAIN_MAX_MDB;
  caps->gain_step_mdb = GAIN_STEP_MDB;
  caps->gain_default_mdb = 0;
}

static int prv_set_gain(const struct pbl_audio_codec *codec, int32_t gain_mdb) {
  return prv_write(
      prv_dev(codec), DA7212_DAC_R_GAIN,
      (uint8_t)(GAIN_CODE_MIN + pbl_audio_gain_to_step(gain_mdb, GAIN_MIN_MDB, GAIN_STEP_MDB)));
}

static int prv_set_mute(const struct pbl_audio_codec *codec, bool mute) {
  return prv_write(prv_dev(codec), DA7212_DAC_FILTERS5, mute ? DA7212_DAC_FILTERS5_SOFTMUTE : 0U);
}

static int prv_enable(const struct pbl_audio_codec *codec, const struct pbl_audio_codec_cfg *cfg) {
  const struct pbl_audio_da7212 *dev = prv_dev(codec);
  uint8_t status;
  int ret;

  if (cfg->rate != RATE || (cfg->fmt & PBL_AUDIO_FMT_I2S) == 0U) {
    return -EINVAL;
  }

  ret = prv_write_seq(dev, s_power_up, ARRAY_LENGTH(s_power_up));
  if (ret != 0) {
    return ret;
  }

  ret = pbl_i2c_reg_read_byte(&dev->i2c, DA7212_PLL_STATUS, &status);
  if (ret != 0) {
    return ret;
  }

  if (status != DA7212_PLL_STATUS_LOCKED) {
    PBL_LOG_ERR("PLL not locked (0x%02x)", status);
    return -EIO;
  }

  ret = prv_write_seq(dev, s_path, ARRAY_LENGTH(s_path));
  if (ret != 0) {
    return ret;
  }

  ret = prv_set_gain(codec, cfg->gain_mdb);
  if (ret != 0) {
    return ret;
  }

  ret = prv_set_mute(codec, cfg->mute);
  if (ret != 0) {
    return ret;
  }

  if ((cfg->fmt & PBL_AUDIO_FMT_CODEC_MASTER) == 0U) {
    return 0;
  }

  return prv_write_seq(dev, s_dai_master, ARRAY_LENGTH(s_dai_master));
}

static int prv_disable(const struct pbl_audio_codec *codec) {
  const struct pbl_audio_da7212 *dev = prv_dev(codec);
  int ret;

  ret = prv_write(dev, DA7212_DAC_FILTERS5, DA7212_DAC_FILTERS5_SOFTMUTE);
  if (ret != 0) {
    return ret;
  }

  return prv_write(dev, DA7212_SYSTEM_ACTIVE, 0U);
}

const struct pbl_audio_codec_ops pbl_audio_da7212_ops = {
  .get_caps = prv_get_caps,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_gain = prv_set_gain,
  .set_mute = prv_set_mute,
};
