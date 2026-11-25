/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/audio/da7212.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DECLARE(driver_audio, CONFIG_DRIVER_AUDIO_LOG_LEVEL);

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
#define DA7212_ALC_CTRL1           0x2bU
#define DA7212_MIXIN_L_SELECT      0x32U
#define DA7212_MIXIN_R_SELECT      0x33U
#define DA7212_ADC_L_GAIN          0x36U
#define DA7212_ADC_R_GAIN          0x37U
#define DA7212_MIC_1_GAIN          0x39U
#define DA7212_DAC_FILTERS5        0x40U
#define DA7212_DAC_R_GAIN          0x46U
#define DA7212_LINE_GAIN           0x4aU
#define DA7212_MIXOUT_R_SELECT     0x4cU
#define DA7212_SYSTEM_MODES_OUTPUT 0x51U
#define DA7212_MICBIAS_CTRL        0x62U
#define DA7212_MIC_1_CTRL          0x63U
#define DA7212_MIXIN_L_CTRL        0x65U
#define DA7212_MIXIN_R_CTRL        0x66U
#define DA7212_ADC_L_CTRL          0x67U
#define DA7212_ADC_R_CTRL          0x68U
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
#define DA7212_PLL_CTRL_EN                0x80U
#define DA7212_PLL_STATUS_LOCK            0x01U
#define DA7212_PLL_STATUS_SRM_LOCK        0x02U
#define DA7212_PLL_STATUS_MCLK            0x04U
#define DA7212_PLL_STATUS_LOCKED          (DA7212_PLL_STATUS_LOCK | DA7212_PLL_STATUS_MCLK)
#define DA7212_SR_16KHZ                   0x05U
#define DA7212_DAC_FILTERS5_SOFTMUTE      0x80U
#define DA7212_DAI_CTRL_EN_16BIT          0x80U
#define DA7212_DAI_CTRL_OE                0x40U
#define DA7212_DAI_CLK_MODE_MASTER_64BCLK 0x81U
#define DA7212_DIG_ROUTING_DAI_ADC        0x10U
#define DA7212_ALC_CTRL1_AUTO_CALIB_EN    0x10U
#define DA7212_MICBIAS_CTRL_MICBIAS1_EN   0x08U
#define DA7212_MIC_1_GAIN_30DB            0x06U
#define DA7212_MIC_1_CTRL_EN              0x80U
#define DA7212_MIC_1_CTRL_MUTE            0x40U
#define DA7212_MIC_1_CTRL_IN_MIC_P        0x04U
#define DA7212_MIXIN_SELECT_L_MIC1        0x02U
#define DA7212_MIXIN_SELECT_R_MIC1        0x04U
#define DA7212_MIXIN_CTRL_ON              0xa8U
#define DA7212_ADC_CTRL_ON                0xa0U
#define DA7212_ADC_CTRL_MUTE              0x40U

// DAC_R_GAIN and ADC_x_GAIN: 0.75 dB steps, 0x6f is 0 dB
#define GAIN_CODE_MIN 0x07U
#define GAIN_CODE_0DB 0x6fU
#define GAIN_CODE_MAX 0x7fU
#define GAIN_STEP_MDB 750
#define GAIN_MIN_MDB  (((int32_t)GAIN_CODE_MIN - (int32_t)GAIN_CODE_0DB) * GAIN_STEP_MDB)
#define GAIN_MAX_MDB  (((int32_t)GAIN_CODE_MAX - (int32_t)GAIN_CODE_0DB) * GAIN_STEP_MDB)

#define WAKE_MS       30U
#define SUPPLY_OFF_MS 100U
#define SUPPLY_ON_MS  10U
#define RESET_MS      10U
#define REFERENCES_MS 30U
#define PLL_LOCK_MS   40U
#define CALIB_POLL_MS 2U
#define CALIB_MS      100U
// Before the capture path set up again after a system mode submit holds
#define MODE_SUBMIT_MS 50U

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
  {DA7212_PLL_CTRL, DA7212_PLL_CTRL_EN, 0U},
  {DA7212_TEST_UNLOCK, 0x8bU, 0U},
  {DA7212_TEST_PLL, 0x03U, 0U},
  {DA7212_TEST_UNLOCK, 0x00U, PLL_LOCK_MS},
};

// Common to both directions: no gain ramp or soft mixing, which delay or ramp every change, and
// the ADCs on the DAI outputs
static const struct prv_reg_seq s_common[] = {
  {DA7212_GAIN_RAMP_CTRL, 0x00U, 0U},
  {DA7212_SR, DA7212_SR_16KHZ, 0U},
  {DA7212_DIG_ROUTING_DAI, DA7212_DIG_ROUTING_DAI_ADC, 0U},
};

// DAI to the right DAC, mixer and line output
static const struct prv_reg_seq s_playback[] = {
  {DA7212_DIG_ROUTING_DAC, 0xbaU, 0U},
  {DA7212_DAC_R_CTRL, 0x80U, 0U},
  {DA7212_MIXOUT_R_SELECT, 0x08U, 0U},
  {DA7212_MIXOUT_R_CTRL, 0x90U, 0U},
  {DA7212_LINE_GAIN, 0x30U, 0U},
  {DA7212_LINE_CTRL, 0x80U, 0U},
  {DA7212_SYSTEM_MODES_OUTPUT, 0x89U, MODE_SUBMIT_MS},
};

static const struct prv_reg_seq s_playback_off[] = {
  {DA7212_DAC_FILTERS5, DA7212_DAC_FILTERS5_SOFTMUTE, 0U},
  {DA7212_LINE_CTRL, 0x00U, 0U},
  {DA7212_MIXOUT_R_CTRL, 0x00U, 0U},
  {DA7212_DAC_R_CTRL, 0x00U, 0U},
};

// Microphone amplifier, muted, to both input mixers and ADCs, muted, for their offset calibration
static const struct prv_reg_seq s_capture[] = {
  {DA7212_MIC_1_GAIN, DA7212_MIC_1_GAIN_30DB, 0U},
  {DA7212_MIC_1_CTRL, DA7212_MIC_1_CTRL_EN | DA7212_MIC_1_CTRL_MUTE | DA7212_MIC_1_CTRL_IN_MIC_P,
   0U},
  {DA7212_MIXIN_L_SELECT, DA7212_MIXIN_SELECT_L_MIC1, 0U},
  {DA7212_MIXIN_R_SELECT, DA7212_MIXIN_SELECT_R_MIC1, 0U},
  {DA7212_MIXIN_L_CTRL, DA7212_MIXIN_CTRL_ON, 0U},
  {DA7212_MIXIN_R_CTRL, DA7212_MIXIN_CTRL_ON, 0U},
  {DA7212_ADC_L_CTRL, DA7212_ADC_CTRL_ON | DA7212_ADC_CTRL_MUTE, 0U},
  {DA7212_ADC_R_CTRL, DA7212_ADC_CTRL_ON | DA7212_ADC_CTRL_MUTE, 0U},
};

static const struct prv_reg_seq s_capture_off[] = {
  {DA7212_ADC_L_CTRL, 0x00U, 0U},   {DA7212_ADC_R_CTRL, 0x00U, 0U},
  {DA7212_MIXIN_L_CTRL, 0x00U, 0U}, {DA7212_MIXIN_R_CTRL, 0x00U, 0U},
  {DA7212_MIC_1_CTRL, 0x00U, 0U},   {DA7212_MICBIAS_CTRL, 0x00U, 0U},
};

static const struct pbl_audio_da7212 *prv_dev(const struct pbl_audio_codec *codec) {
  return PBL_CONTAINER_OF(codec, const struct pbl_audio_da7212, codec);
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

// A PLL that ran with SRM as DAI master, as older firmware did, keeps a wrong rate until the codec
// loses power, and reports SRM lock on the first read
static int prv_recover(const struct pbl_audio_da7212 *dev) {
  uint8_t status;
  int ret;

  ret = prv_write(dev, DA7212_SYSTEM_ACTIVE, DA7212_SYSTEM_ACTIVE_ON);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(WAKE_MS);

  ret = pbl_i2c_reg_read_byte(&dev->i2c, DA7212_PLL_STATUS, &status);
  if (ret != 0) {
    return ret;
  }

  if ((status & DA7212_PLL_STATUS_SRM_LOCK) == 0U) {
    return 0;
  }

  PBL_LOG_WRN("PLL left in SRM, power cycling the codec");

  ret = pbl_regulator_power_cycle(dev->vdd, SUPPLY_OFF_MS);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(SUPPLY_ON_MS);

  return 0;
}

static const uint16_t s_micbias_mv[] = {1600U, 2200U, 2500U, 3000U};

static int prv_micbias_level(uint16_t mv) {
  for (size_t i = 0U; i < PBL_ARRAY_SIZE(s_micbias_mv); i++) {
    if (s_micbias_mv[i] == mv) {
      return (int)i;
    }
  }

  return -EINVAL;
}

int pbl_audio_da7212_init(const struct pbl_device *dev) {
  const struct pbl_audio_da7212 *da7212 =
      PBL_CONTAINER_OF(dev, const struct pbl_audio_da7212, codec.dev);
  int ret;

  if (da7212->micbias1_mv != 0U && prv_micbias_level(da7212->micbias1_mv) < 0) {
    return -EINVAL;
  }

  ret = pbl_device_init(&da7212->vdd->dev);
  if (ret != 0) {
    return ret;
  }

  ret = prv_recover(da7212);
  if (ret != 0) {
    return ret;
  }

  return prv_write(da7212, DA7212_SYSTEM_ACTIVE, 0U);
}

static void prv_get_caps(const struct pbl_audio_codec *codec, enum pbl_audio_dir dir,
                         struct pbl_audio_caps *caps) {
  // Recording needs the microphone bias
  if (dir == PBL_AUDIO_CAPTURE && prv_dev(codec)->micbias1_mv == 0U) {
    caps->rates = 0U;
  }

  caps->gain_min_mdb = GAIN_MIN_MDB;
  caps->gain_max_mdb = GAIN_MAX_MDB;
  caps->gain_step_mdb = GAIN_STEP_MDB;
  caps->gain_default_mdb = 0;
}

static int prv_set_gain(const struct pbl_audio_codec *codec, enum pbl_audio_dir dir,
                        int32_t gain_mdb) {
  const struct pbl_audio_da7212 *dev = prv_dev(codec);
  const uint8_t code =
      (uint8_t)(GAIN_CODE_MIN + pbl_audio_gain_to_step(gain_mdb, GAIN_MIN_MDB, GAIN_STEP_MDB));
  int ret;

  if (dir == PBL_AUDIO_PLAYBACK) {
    return prv_write(dev, DA7212_DAC_R_GAIN, code);
  }

  ret = prv_write(dev, DA7212_ADC_L_GAIN, code);
  if (ret != 0) {
    return ret;
  }

  return prv_write(dev, DA7212_ADC_R_GAIN, code);
}

static int prv_set_mute(const struct pbl_audio_codec *codec, enum pbl_audio_dir dir, bool mute) {
  const struct pbl_audio_da7212 *dev = prv_dev(codec);
  const uint8_t adc = DA7212_ADC_CTRL_ON | (mute ? DA7212_ADC_CTRL_MUTE : 0U);
  int ret;

  if (dir == PBL_AUDIO_PLAYBACK) {
    return prv_write(dev, DA7212_DAC_FILTERS5, mute ? DA7212_DAC_FILTERS5_SOFTMUTE : 0U);
  }

  ret = prv_write(dev, DA7212_ADC_L_CTRL, adc);
  if (ret != 0) {
    return ret;
  }

  return prv_write(dev, DA7212_ADC_R_CTRL, adc);
}

static int prv_power_up(const struct pbl_audio_da7212 *dev) {
  uint8_t status;
  int ret;

  ret = prv_write_seq(dev, s_power_up, PBL_ARRAY_SIZE(s_power_up));
  if (ret != 0) {
    return ret;
  }

  ret = pbl_i2c_reg_read_byte(&dev->i2c, DA7212_PLL_STATUS, &status);
  if (ret != 0) {
    return ret;
  }

  if ((status & DA7212_PLL_STATUS_LOCKED) != DA7212_PLL_STATUS_LOCKED) {
    PBL_LOG_ERR("PLL not locked (0x%02x)", status);
    return -EIO;
  }

  return prv_write_seq(dev, s_common, PBL_ARRAY_SIZE(s_common));
}

static bool prv_calibrated(const struct pbl_audio_da7212 *dev) {
  uint8_t val;

  // A failed read ends the wait; the writes after it fail too
  return pbl_i2c_reg_read_byte(&dev->i2c, DA7212_ALC_CTRL1, &val) != 0 ||
         (val & DA7212_ALC_CTRL1_AUTO_CALIB_EN) == 0U;
}

// Microphone to the ADCs, with their offset calibrated, the ADCs muted at their last gain
static int prv_capture_path_up(const struct pbl_audio_da7212 *dev) {
  int ret;

  if (dev->micbias1_mv == 0U) {
    return -ENOTSUP;
  }

  ret = prv_write(dev, DA7212_MICBIAS_CTRL,
                  DA7212_MICBIAS_CTRL_MICBIAS1_EN | (uint8_t)prv_micbias_level(dev->micbias1_mv));
  if (ret != 0) {
    return ret;
  }

  ret = prv_write_seq(dev, s_capture, PBL_ARRAY_SIZE(s_capture));
  if (ret != 0) {
    return ret;
  }

  // Cancel the DC offset of the ADCs, which clears the bit once done
  ret = prv_write(dev, DA7212_ALC_CTRL1, DA7212_ALC_CTRL1_AUTO_CALIB_EN);
  if (ret != 0) {
    return ret;
  }

  if (PBL_POLL_SLEEP_MS(prv_calibrated(dev), CALIB_POLL_MS, CALIB_MS) != 0) {
    PBL_LOG_WRN("ADC calibration timed out");
  }

  return prv_write(dev, DA7212_MIC_1_CTRL, DA7212_MIC_1_CTRL_EN | DA7212_MIC_1_CTRL_IN_MIC_P);
}

static int prv_capture_up(const struct pbl_audio_da7212 *dev,
                          const struct pbl_audio_codec_cfg *cfg) {
  int ret;

  ret = prv_capture_path_up(dev);
  if (ret != 0) {
    return ret;
  }

  ret = prv_set_gain(&dev->codec, PBL_AUDIO_CAPTURE, cfg->gain_mdb);
  if (ret != 0) {
    return ret;
  }

  return prv_set_mute(&dev->codec, PBL_AUDIO_CAPTURE, cfg->mute);
}

// ADC gain and mute, kept while the capture path is set up again
static const uint8_t s_capture_regs[] = {
  DA7212_ADC_L_GAIN,
  DA7212_ADC_R_GAIN,
  DA7212_ADC_L_CTRL,
  DA7212_ADC_R_CTRL,
};

static int prv_playback_up(const struct pbl_audio_da7212 *dev,
                           const struct pbl_audio_codec_cfg *cfg) {
  const bool capturing = (cfg->streaming & PBL_BIT(PBL_AUDIO_CAPTURE)) != 0U;
  uint8_t saved[PBL_ARRAY_SIZE(s_capture_regs)];
  int ret;

  for (size_t i = 0U; capturing && i < PBL_ARRAY_SIZE(s_capture_regs); i++) {
    ret = pbl_i2c_reg_read_byte(&dev->i2c, s_capture_regs[i], &saved[i]);
    if (ret != 0) {
      return ret;
    }
  }

  ret = prv_write_seq(dev, s_playback, PBL_ARRAY_SIZE(s_playback));
  if (ret != 0) {
    return ret;
  }

  // Submitting the output system mode powers the capture path down: bring it up again
  if (capturing) {
    ret = prv_capture_path_up(dev);
    for (size_t i = 0U; ret == 0 && i < PBL_ARRAY_SIZE(s_capture_regs); i++) {
      ret = prv_write(dev, s_capture_regs[i], saved[i]);
    }
    if (ret != 0) {
      return ret;
    }
  }

  ret = prv_set_gain(&dev->codec, PBL_AUDIO_PLAYBACK, cfg->gain_mdb);
  if (ret != 0) {
    return ret;
  }

  return prv_set_mute(&dev->codec, PBL_AUDIO_PLAYBACK, cfg->mute);
}

// The DAI drives its data output while recording
static int prv_dai_update(const struct pbl_audio_da7212 *dev, uint8_t dirs) {
  const bool capture = (dirs & PBL_BIT(PBL_AUDIO_CAPTURE)) != 0U;

  return prv_write(dev, DA7212_DAI_CTRL,
                   DA7212_DAI_CTRL_EN_16BIT | (capture ? DA7212_DAI_CTRL_OE : 0U));
}

// Power down the path of dir, and the codec once nothing streams
static int prv_disable(const struct pbl_audio_codec *codec, enum pbl_audio_dir dir,
                       uint8_t streaming) {
  const struct pbl_audio_da7212 *dev = prv_dev(codec);
  int ret;
  int err;

  if (dir == PBL_AUDIO_CAPTURE) {
    ret = prv_write_seq(dev, s_capture_off, PBL_ARRAY_SIZE(s_capture_off));
  } else if (streaming != 0U) {
    ret = prv_write_seq(dev, s_playback_off, PBL_ARRAY_SIZE(s_playback_off));
  } else {
    ret = prv_write(dev, DA7212_DAC_FILTERS5, DA7212_DAC_FILTERS5_SOFTMUTE);
  }

  err = streaming != 0U ? prv_dai_update(dev, streaming) : prv_write(dev, DA7212_SYSTEM_ACTIVE, 0U);

  return ret != 0 ? ret : err;
}

// The first direction powers the codec up; the other then shares its clocks
static int prv_enable_path(const struct pbl_audio_da7212 *dev,
                           const struct pbl_audio_codec_cfg *cfg) {
  const bool first = cfg->streaming == 0U;
  const uint8_t dirs = cfg->streaming | PBL_BIT(cfg->dir);
  int ret;

  if (first) {
    ret = prv_power_up(dev);
    if (ret != 0) {
      return ret;
    }
  }

  // The DAI, then its bit and frame clocks as master, run before the ADC offset calibration,
  // which needs them
  ret = prv_dai_update(dev, dirs);
  if (ret == 0 && first && (cfg->fmt & PBL_AUDIO_FMT_CODEC_MASTER) != 0U) {
    ret = prv_write(dev, DA7212_DAI_CLK_MODE, DA7212_DAI_CLK_MODE_MASTER_64BCLK);
  }
  if (ret != 0) {
    return ret;
  }

  ret = cfg->dir == PBL_AUDIO_PLAYBACK ? prv_playback_up(dev, cfg) : prv_capture_up(dev, cfg);
  if (ret != 0) {
    return ret;
  }

  // Again, as submitting the output system mode may have changed it
  return prv_dai_update(dev, dirs);
}

static int prv_enable(const struct pbl_audio_codec *codec, const struct pbl_audio_codec_cfg *cfg) {
  const struct pbl_audio_da7212 *dev = prv_dev(codec);
  int ret;

  if (cfg->rate != RATE || (cfg->fmt & PBL_AUDIO_FMT_I2S) == 0U) {
    return -EINVAL;
  }

  ret = prv_enable_path(dev, cfg);
  if (ret != 0) {
    (void)prv_disable(codec, cfg->dir, cfg->streaming);
  }

  return ret;
}

const struct pbl_audio_codec_ops pbl_audio_da7212_ops = {
  .get_caps = prv_get_caps,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_gain = prv_set_gain,
  .set_mute = prv_set_mute,
};

#include PBL_DT_INSTANCES(dlg_da7212)
