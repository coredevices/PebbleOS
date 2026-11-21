/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/audio/wm8994.h>
#include <pbl/kernel/delay.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#define REG_RESET          0x0000U
#define REG_PM1            0x0001U
#define REG_PM5            0x0005U
#define REG_HPOUT1L_VOL    0x001cU
#define REG_HPOUT1R_VOL    0x001dU
#define REG_OUT_MIXER1     0x002dU
#define REG_OUT_MIXER2     0x002eU
#define REG_ANTIPOP2       0x0039U
#define REG_WSEQ_CTRL1     0x0110U
#define REG_AIF1_CLOCKING1 0x0200U
#define REG_CLOCKING1      0x0208U
#define REG_AIF1_RATE      0x0210U
#define REG_AIF1_CTRL1     0x0300U
#define REG_AIF1_MS        0x0302U
#define REG_AIF1_DAC1_F1   0x0420U
#define REG_AIF1_DAC2_F1   0x0422U

#define ID 0x8994U

// AIF1 16-bit I2S, the right ADC channel from the right input
#define AIF1_CTRL1_I2S_16 0x4010U

// The DAC filters ramp the mute in and out
#define DAC_MUTE   0x0200U
#define DAC_UNMUTE 0x0010U

// Headphone volume: 1 dB steps from -57 dB to +6 dB, updated together, unmuted
#define HP_VOL_MIN_MDB     (-57000)
#define HP_VOL_MAX_MDB     6000
#define HP_VOL_STEP_MDB    1000
#define HP_VOL_DEFAULT_MDB (-12000)
#define HP_VOL_VU          0x0100U
#define HP_VOL_MUTE_N      0x0040U

// The bias settles in 50 ms, the cold start of the headphone sequence with its DC servo in 325
#define BIAS_MS     50U
#define HP_START_MS 325U

struct prv_write {
  uint16_t reg;
  uint16_t val;
};

// The rate in AIF1 with the clock at 256 times it
static const struct {
  uint32_t rate;
  uint16_t val;
} s_rates[] = {
  {8000U, 0x0003U},
  {16000U, 0x0033U},
  {32000U, 0x0063U},
  {48000U, 0x0083U},
};

// Errata, then the bias and VMID
static const struct prv_write s_power[] = {
  {0x0102U, 0x0003U},      {0x0817U, 0x0000U}, {0x0102U, 0x0000U},
  {REG_ANTIPOP2, 0x006cU}, {REG_PM1, 0x0003U},
};

// AIF1 DAC1 to DAC1, the slave of the link, on MCLK1
static const struct prv_write s_path[] = {
  {REG_PM5, 0x0303U}, {0x0601U, 0x0001U}, {0x0602U, 0x0001U},
  {0x0604U, 0x0000U}, {0x0605U, 0x0000U},
};

static const struct prv_write s_clocks[] = {
  {REG_AIF1_CTRL1, AIF1_CTRL1_I2S_16}, {REG_AIF1_MS, 0x0000U},    {REG_CLOCKING1, 0x000aU},
  {REG_AIF1_CLOCKING1, 0x0001U},       {REG_OUT_MIXER1, 0x0100U}, {REG_OUT_MIXER2, 0x0100U},
  {REG_WSEQ_CTRL1, 0x8100U},
};

// DAC1 and DAC2 digital volumes at 0 dB, unmuted
static const struct prv_write s_dac[] = {
  {REG_AIF1_DAC1_F1, 0x0000U},    {0x0610U, 0x00c0U}, {0x0611U, 0x00c0U},
  {REG_AIF1_DAC1_F1, DAC_UNMUTE}, {0x0612U, 0x00c0U}, {0x0613U, 0x00c0U},
  {REG_AIF1_DAC2_F1, DAC_UNMUTE},
};

static const struct pbl_audio_wm8994 *prv_dev(const struct pbl_audio_codec *codec) {
  return PBL_CONTAINER_OF(codec, const struct pbl_audio_wm8994, codec);
}

// Registers have 16-bit addresses and values, most significant byte first
static int prv_write(const struct pbl_audio_wm8994 *wm, uint16_t reg, uint16_t val) {
  uint8_t buf[4];

  pbl_put_be16(reg, &buf[0]);
  pbl_put_be16(val, &buf[2]);

  return pbl_i2c_write(&wm->i2c, buf, sizeof(buf));
}

static int prv_read(const struct pbl_audio_wm8994 *wm, uint16_t reg, uint16_t *val) {
  uint8_t addr[2];
  uint8_t buf[2];
  int ret;

  pbl_put_be16(reg, addr);

  ret = pbl_i2c_write_read(&wm->i2c, addr, sizeof(addr), buf, sizeof(buf));
  if (ret != 0) {
    return ret;
  }

  *val = pbl_get_be16(buf);

  return 0;
}

static int prv_run(const struct pbl_audio_wm8994 *wm, const struct prv_write *seq, size_t len) {
  int ret;

  for (size_t i = 0U; i < len; i++) {
    ret = prv_write(wm, seq[i].reg, seq[i].val);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

static int prv_set_gain_reg(const struct pbl_audio_wm8994 *wm, int32_t gain_mdb) {
  const uint16_t vol = (uint16_t)pbl_audio_gain_to_step(gain_mdb, HP_VOL_MIN_MDB, HP_VOL_STEP_MDB);
  int ret;

  ret = prv_write(wm, REG_HPOUT1L_VOL, HP_VOL_MUTE_N | vol);
  if (ret != 0) {
    return ret;
  }

  return prv_write(wm, REG_HPOUT1R_VOL, HP_VOL_VU | HP_VOL_MUTE_N | vol);
}

static int prv_set_mute_reg(const struct pbl_audio_wm8994 *wm, bool mute) {
  int ret;

  ret = prv_write(wm, REG_AIF1_DAC1_F1, mute ? DAC_MUTE : DAC_UNMUTE);
  if (ret != 0) {
    return ret;
  }

  return prv_write(wm, REG_AIF1_DAC2_F1, mute ? DAC_MUTE : DAC_UNMUTE);
}

// The reset powers everything down
static int prv_power_down(const struct pbl_audio_wm8994 *wm) {
  return prv_write(wm, REG_RESET, 0x0000U);
}

static void prv_get_caps(const struct pbl_audio_codec *codec, struct pbl_audio_caps *caps) {
  caps->gain_min_mdb = HP_VOL_MIN_MDB;
  caps->gain_max_mdb = HP_VOL_MAX_MDB;
  caps->gain_step_mdb = HP_VOL_STEP_MDB;
  caps->gain_default_mdb = HP_VOL_DEFAULT_MDB;
}

static int prv_enable_seq(const struct pbl_audio_wm8994 *wm, const struct pbl_audio_codec_cfg *cfg,
                          uint16_t rate) {
  int ret;

  ret = prv_run(wm, s_power, PBL_ARRAY_SIZE(s_power));
  if (ret != 0) {
    return ret;
  }
  pbl_sleep_ms(BIAS_MS);

  ret = prv_run(wm, s_path, PBL_ARRAY_SIZE(s_path));
  if (ret != 0) {
    return ret;
  }

  ret = prv_write(wm, REG_AIF1_RATE, rate);
  if (ret != 0) {
    return ret;
  }

  ret = prv_run(wm, s_clocks, PBL_ARRAY_SIZE(s_clocks));
  if (ret != 0) {
    return ret;
  }
  pbl_sleep_ms(HP_START_MS);

  ret = prv_run(wm, s_dac, PBL_ARRAY_SIZE(s_dac));
  if (ret != 0) {
    return ret;
  }

  ret = prv_set_gain_reg(wm, cfg->gain_mdb);
  if (ret != 0) {
    return ret;
  }

  return prv_set_mute_reg(wm, cfg->mute);
}

// From reset, with the master clock running: the headphone sequence starts cold every time
static int prv_enable(const struct pbl_audio_codec *codec, const struct pbl_audio_codec_cfg *cfg) {
  const struct pbl_audio_wm8994 *wm = prv_dev(codec);
  uint16_t rate = 0U;
  int ret;

  for (size_t i = 0U; i < PBL_ARRAY_SIZE(s_rates); i++) {
    if (s_rates[i].rate == cfg->rate) {
      rate = s_rates[i].val;
    }
  }

  if (rate == 0U || (cfg->fmt & PBL_AUDIO_FMT_I2S) == 0U ||
      (cfg->fmt & PBL_AUDIO_FMT_CODEC_MASTER) != 0U) {
    return -EINVAL;
  }

  ret = prv_enable_seq(wm, cfg, rate);
  if (ret != 0) {
    (void)prv_power_down(wm);
  }

  return ret;
}

static int prv_disable(const struct pbl_audio_codec *codec) {
  const struct pbl_audio_wm8994 *wm = prv_dev(codec);
  int ret;

  ret = prv_set_mute_reg(wm, true);
  if (ret != 0) {
    return ret;
  }

  return prv_power_down(wm);
}

static int prv_set_gain(const struct pbl_audio_codec *codec, int32_t gain_mdb) {
  return prv_set_gain_reg(prv_dev(codec), gain_mdb);
}

static int prv_set_mute(const struct pbl_audio_codec *codec, bool mute) {
  return prv_set_mute_reg(prv_dev(codec), mute);
}

int pbl_audio_wm8994_init(const struct pbl_device *dev) {
  const struct pbl_audio_wm8994 *wm =
      PBL_CONTAINER_OF(dev, const struct pbl_audio_wm8994, codec.dev);
  uint16_t id;
  int ret;

  ret = prv_read(wm, REG_RESET, &id);
  if (ret != 0) {
    return ret;
  }

  if (id != ID) {
    return -ENODEV;
  }

  return prv_power_down(wm);
}

const struct pbl_audio_codec_ops pbl_audio_wm8994_ops = {
  .get_caps = prv_get_caps,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_gain = prv_set_gain,
  .set_mute = prv_set_mute,
};

#include PBL_DT_INSTANCES(wlf_wm8994)
