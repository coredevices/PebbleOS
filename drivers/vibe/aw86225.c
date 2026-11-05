/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/vibe.h>
#include <pbl/drivers/vibe/aw86225.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DEFINE(driver_vibe_aw86225, CONFIG_DRIVER_VIBE_LOG_LEVEL);

#define AW862XX_REG_SRST      (0x00)
#define AW862XX_REG_PLAYCFG2  (0x07)
#define AW862XX_REG_PLAYCFG3  (0x08)
#define AW862XX_REG_PLAYCFG4  (0x09)
#define AW862XX_REG_WAVCFG1   (0x0A)
#define AW862XX_REG_WAVCFG2   (0x0B)
#define AW862XX_REG_WAVCFG9   (0x12)
#define AW862XX_REG_CONTCFG1  (0x18)
#define AW862XX_REG_CONTCFG2  (0x19)
#define AW862XX_REG_CONTCFG3  (0x1A)
#define AW862XX_REG_CONTCFG6  (0x1D)
#define AW862XX_REG_CONTCFG7  (0x1E)
#define AW862XX_REG_CONTCFG8  (0x1F)
#define AW862XX_REG_CONTCFG9  (0x20)
#define AW862XX_REG_CONTCFG10 (0x21)
#define AW862XX_REG_CONTCFG11 (0x22)
#define AW862XX_REG_CONTRD14  (0x25)
#define AW862XX_REG_CONTRD15  (0x26)
#define AW862XX_REG_CONTRD16  (0x27)
#define AW862XX_REG_CONTRD17  (0x28)
#define AW862XX_REG_RTPCFG1   (0x2D)
#define AW862XX_REG_RTPCFG2   (0x2E)
#define AW862XX_REG_RTPCFG3   (0x2F)
#define AW862XX_REG_GLBRD5    (0x3F)
#define AW862XX_REG_RAMADDRH  (0x40)
#define AW862XX_REG_RAMDATA   (0x42)
#define AW862XX_REG_SYSCTRL1  (0x43)
#define AW862XX_REG_SYSCTRL2  (0x44)
#define AW862XX_REG_SYSCTRL7  (0x49)
#define AW862XX_REG_DETCFG2   (0x52)
#define AW862XX_REG_DET_VBAT  (0x55)
#define AW862XX_REG_DET_LO    (0x57)
#define AW862XX_REG_TRIMCFG3  (0x5A)
#define AW862XX_REG_CHIPID    (0x64)

#define AW862XX_PLAYCFG3_BRK_EN         PBL_BIT(2)
#define AW862XX_PLAYCFG3_PLAY_MODE_MASK PBL_GENMASK(1, 0)
#define AW862XX_PLAYCFG3_PLAY_MODE_RAM  0x0U
#define AW862XX_PLAYCFG3_PLAY_MODE_CONT 0x2U
#define AW862XX_PLAYCFG3_PLAY_MODE_STOP 0x3U

/* PLAYCFG4: reg 0x09 RW */
#define AW862XX_PLAYCFG4_STOP PBL_BIT(1)
#define AW862XX_PLAYCFG4_GO   PBL_BIT(0)

#define AW862XX_F0_CALI_LSB_PERMYRIAD   (24)
#define AW862XX_CONTCFG1_EDGE_FREQ_NONE (0x00)
#define AW862XX_CONTCFG1_SIN_MODE_COS   PBL_BIT(0)
#define AW862XX_CONTCFG1_EN_F0_DET      PBL_BIT(3)
#define AW862XX_CONTCFG2_CONF_F0        (24000U / CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ)
#define AW862XX_CONTCFG3_F0_DET_DRV_WIDTH \
  (24000U / CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ - 8U - 8U - 15U)
#define AW862XX_CONTCFG7_FULL_SCALE       (0x7FL)
#define AW862XX_CONTCFG8_F0_DET_DRV1_TIME (0x04U)
#define AW862XX_CONTCFG9_F0_DET_DRV2_TIME (0x14U)
#define AW862XX_CONTCFG10_BRK_TIME        (0x08U)
#define AW862XX_CONTCFG11_TRACK_MARGIN    (0x0FU)
#define AW862XX_CONTCFG6_TRACK_EN         PBL_BIT(7)
#define AW862XX_CONTCFG6_DRV1_LVL_MASK    PBL_GENMASK(6, 0)
#define AW862XX_RAM_BASE_ADDR             (0x0800U)
#define AW862XX_RAM_HEADER_VERSION        (0x01U)
#define AW862XX_RAM_HEADER_LEN            (1U + 4U)
#define AW862XX_RAM_WAVEFORM              (1U)
#define AW862XX_WAVCFG_END                (0U)
#define AW862XX_WAVCFG9_SEQ1LOOP_MASK     PBL_GENMASK(7, 4)
#define AW862XX_WAVCFG9_LOOP_INFINITE     (0x0FU)
#define AW862XX_PLAYCFG2_GAIN_UNITY       (0x80U)
#define AW862XX_RMS_TO_PEAK_MILLI         (1414U)
#define AW862XX_VBAT_REFER_MV             (4200U)
#define AW862XX_VBAT_MIN_MV               (3000U)
#define AW862XX_VBAT_MAX_MV               (4500U)
#define AW862XX_VBAT_FULL_SCALE_MV        (6100U)
#define AW862XX_VBAT_CODE_MAX             (1024U)
#define AW862XX_PLAYCFG2_GAIN_LIMIT \
  (AW862XX_PLAYCFG2_GAIN_UNITY * AW862XX_VBAT_REFER_MV / AW862XX_VBAT_MIN_MV)
#define AW862XX_RTPCFG1_ADDRH_MASK        PBL_GENMASK(3, 0)
#define AW862XX_GLBRD5_STATE_MASK         PBL_GENMASK(3, 0)
#define AW862XX_GLBRD5_STATE_STANDBY      (0x00)
#define AW862XX_TRIMCFG3_TRIM_LRA_MASK    PBL_GENMASK(5, 0)
#define AW862XX_SYSCTRL1_RAMINIT          PBL_BIT(3)
#define AW862XX_SYSCTRL2_STANDBY          PBL_BIT(6)
#define AW862XX_SYSCTRL2_WAVDAT_MODE_MASK PBL_GENMASK(1, 0)
#define AW862XX_SYSCTRL2_RATE_12K         0x2U
#define AW862XX_SYSCTRL7_GAIN_CHANGEABLE  PBL_BIT(6)
#define AW862XX_DETCFG2_VBAT_GO           PBL_BIT(1)
#define AW862XX_DET_LO_VBAT_MASK          PBL_GENMASK(5, 4)

#define AW862XX_PWR_OFF_TIME           (2) /* ms */
#define AW862XX_PWR_ON_TIME            (8) /* ms */
#define AW862XX_VBAT_DET_TIME          (3) /* ms */
#define AW862XX_STOP_STANDBY_RETRIES   (40)
#define AW862XX_STOP_STANDBY_POLL_MS   (2)
#define AW862XX_F0_DET_STANDBY_RETRIES (200)
#define AW862XX_F0_DET_STANDBY_POLL_MS (10)

#define AW862XX_OUTPUT_FULL_SCALE_MV \
  (AW862XX_VBAT_REFER_MV * CONFIG_VIBE_AW86225_OUTPUT_GAIN_PERCENT / 100U)

static_assert(CONFIG_VIBE_AW86225_RATED_VOLTAGE_MV * AW862XX_RMS_TO_PEAK_MILLI / 1000U <=
                  AW862XX_OUTPUT_FULL_SCALE_MV,
              "rated voltage exceeds the full-scale output");

//! One full-scale LRA cycle at 12 kS/s.
static const uint8_t s_sine_cycle[] = {
  0x00, 0x10, 0x20, 0x2f, 0x3d, 0x4b, 0x57, 0x62, 0x6b, 0x73, 0x79, 0x7d, 0x7f,
  0x7f, 0x7d, 0x79, 0x73, 0x6b, 0x62, 0x57, 0x4b, 0x3d, 0x2f, 0x20, 0x10, 0x00,
  0xf0, 0xe0, 0xd1, 0xc3, 0xb5, 0xa9, 0x9e, 0x95, 0x8d, 0x87, 0x83, 0x81, 0x81,
  0x83, 0x87, 0x8d, 0x95, 0x9e, 0xa9, 0xb5, 0xc3, 0xd1, 0xe0, 0xf0,
};

static const struct pbl_aw86225 *prv_get(const struct pbl_vibe *vibe) {
  return container_of(vibe, const struct pbl_aw86225, vibe);
}

static int prv_read_register(const struct pbl_aw86225 *dev, uint8_t reg, uint8_t *val) {
  int ret;

  ret = pbl_i2c_reg_read_byte(&dev->i2c, reg, val);

  return ret;
}

static int prv_write_register_block(const struct pbl_aw86225 *dev, uint8_t reg, const uint8_t *data,
                                    size_t length) {
  int ret;

  ret = pbl_i2c_burst_write(&dev->i2c, reg, data, length);

  return ret;
}

static int prv_write_register(const struct pbl_aw86225 *dev, uint8_t reg, uint8_t val) {
  return prv_write_register_block(dev, reg, &val, 1);
}

static int prv_update_bits(const struct pbl_aw86225 *dev, uint8_t reg, uint8_t mask, uint8_t val) {
  uint8_t reg_val;
  int ret;

  ret = prv_read_register(dev, reg, &reg_val);
  if (ret != 0) {
    return ret;
  }

  return prv_write_register(dev, reg, (reg_val & ~mask) | (val & mask));
}

static int prv_set_raminit(const struct pbl_aw86225 *dev, bool enable) {
  return prv_update_bits(dev, AW862XX_REG_SYSCTRL1, AW862XX_SYSCTRL1_RAMINIT,
                         enable ? AW862XX_SYSCTRL1_RAMINIT : 0U);
}

static int prv_is_standby(const struct pbl_aw86225 *dev, bool *standby) {
  uint8_t val;
  int ret;

  ret = prv_read_register(dev, AW862XX_REG_GLBRD5, &val);
  if (ret != 0) {
    return ret;
  }

  *standby = PBL_FIELD_GET(AW862XX_GLBRD5_STATE_MASK, val) == AW862XX_GLBRD5_STATE_STANDBY;

  return 0;
}

static int prv_wait_for_standby(const struct pbl_aw86225 *dev, unsigned int retries,
                                uint32_t poll_ms) {
  bool standby;
  int ret;

  for (unsigned int i = 0; i < retries; i++) {
    ret = prv_is_standby(dev, &standby);
    if (ret != 0) {
      return ret;
    }

    if (standby) {
      return 0;
    }

    pbl_sleep_ms(poll_ms);
  }

  return -ETIMEDOUT;
}

static int prv_stop_command(const struct pbl_aw86225 *dev) {
  int ret;

  ret = prv_set_raminit(dev, true);
  if (ret != 0) {
    return ret;
  }

  ret = prv_update_bits(
      dev, AW862XX_REG_PLAYCFG3, AW862XX_PLAYCFG3_PLAY_MODE_MASK,
      PBL_FIELD_PREP(AW862XX_PLAYCFG3_PLAY_MODE_MASK, AW862XX_PLAYCFG3_PLAY_MODE_STOP));
  if (ret != 0) {
    return ret;
  }

  ret = prv_write_register(dev, AW862XX_REG_PLAYCFG4, AW862XX_PLAYCFG4_GO);
  if (ret != 0) {
    return ret;
  }

  return prv_set_raminit(dev, false);
}

//! The waveform is an infinite hardware loop, so a stop that silently fails leaves the motor
//! running: only report success once the chip is in standby.
static int prv_stop_playback(const struct pbl_aw86225 *dev) {
  bool standby;
  int ret;

  ret = prv_stop_command(dev);
  if (ret == 0) {
    ret = prv_wait_for_standby(dev, AW862XX_STOP_STANDBY_RETRIES, AW862XX_STOP_STANDBY_POLL_MS);
  }
  if (ret == 0) {
    return 0;
  }

  ret = prv_update_bits(dev, AW862XX_REG_SYSCTRL2, AW862XX_SYSCTRL2_STANDBY,
                        AW862XX_SYSCTRL2_STANDBY);
  if (ret != 0) {
    return ret;
  }

  ret = prv_update_bits(dev, AW862XX_REG_SYSCTRL2, AW862XX_SYSCTRL2_STANDBY, 0U);
  if (ret != 0) {
    return ret;
  }

  ret = prv_is_standby(dev, &standby);
  if (ret != 0) {
    return ret;
  }

  return standby ? 0 : -ETIMEDOUT;
}

static uint8_t prv_gain_for_strength(const struct pbl_aw86225 *dev, uint8_t strength) {
  const uint32_t vbat_mv =
      (dev->state->vbat_mv != 0U) ? dev->state->vbat_mv : AW862XX_VBAT_REFER_MV;
  uint32_t gain =
      (uint32_t)strength * AW862XX_PLAYCFG2_GAIN_UNITY * AW862XX_VBAT_REFER_MV / (100U * vbat_mv);

  if (gain > AW862XX_PLAYCFG2_GAIN_LIMIT) {
    gain = AW862XX_PLAYCFG2_GAIN_LIMIT;
  }

  return gain;
}

static int prv_read_vbat_code(const struct pbl_aw86225 *dev, uint32_t *code) {
  uint8_t hi;
  uint8_t lo;
  int ret;

  ret = prv_update_bits(dev, AW862XX_REG_DETCFG2, AW862XX_DETCFG2_VBAT_GO, AW862XX_DETCFG2_VBAT_GO);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(AW862XX_VBAT_DET_TIME);

  ret = prv_read_register(dev, AW862XX_REG_DET_VBAT, &hi);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_register(dev, AW862XX_REG_DET_LO, &lo);
  if (ret != 0) {
    return ret;
  }

  *code = ((uint32_t)hi << 2) | PBL_FIELD_GET(AW862XX_DET_LO_VBAT_MASK, lo);

  return 0;
}

//! Playback must be stopped.
static int prv_update_vbat(const struct pbl_aw86225 *dev) {
  uint32_t vbat_mv;
  uint32_t code = 0U;
  int ret;
  int ret_raminit;

  ret = prv_set_raminit(dev, true);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_vbat_code(dev, &code);
  ret_raminit = prv_set_raminit(dev, false);
  if (ret != 0) {
    return ret;
  }
  if (ret_raminit != 0) {
    return ret_raminit;
  }

  vbat_mv = code * AW862XX_VBAT_FULL_SCALE_MV / AW862XX_VBAT_CODE_MAX;
  if (vbat_mv < AW862XX_VBAT_MIN_MV) {
    vbat_mv = AW862XX_VBAT_MIN_MV;
  } else if (vbat_mv > AW862XX_VBAT_MAX_MV) {
    vbat_mv = AW862XX_VBAT_MAX_MV;
  }

  dev->state->vbat_mv = vbat_mv;
  PBL_LOG_DBG("VBAT %u mV, full-strength gain %u", (unsigned int)vbat_mv,
              prv_gain_for_strength(dev, VIBE_STRENGTH_MAX));

  return 0;
}

//! With the gain compensated to AW862XX_VBAT_REFER_MV a full-scale sample
//! nominally drives that voltage; the measured output gain corrects it.
static void prv_scale_cycle(uint8_t *dst, uint32_t rms_mv) {
  const int32_t peak_mv = rms_mv * AW862XX_RMS_TO_PEAK_MILLI / 1000U;
  const int32_t full_scale_mv = AW862XX_OUTPUT_FULL_SCALE_MV;

  for (size_t i = 0; i < sizeof(s_sine_cycle); ++i) {
    int32_t sample = (int8_t)s_sine_cycle[i] * peak_mv;

    sample = (sample + (sample < 0 ? -full_scale_mv : full_scale_mv) / 2) / full_scale_mv;
    dst[i] = (uint8_t)(int8_t)sample;
  }
}

static int prv_write_ram_image(const struct pbl_aw86225 *dev) {
  const uint16_t base = AW862XX_RAM_BASE_ADDR;
  const uint16_t start = base + AW862XX_RAM_HEADER_LEN;
  const uint16_t end = start + sizeof(s_sine_cycle) - 1;
  const uint8_t addr[] = {base >> 8, base & 0xFF};
  const uint8_t fifo[] = {
    (((base >> 1) >> 4) & 0xF0) | (((base - (base >> 2)) >> 8) & 0x0F),
    (base >> 1) & 0xFF,
    (base - (base >> 2)) & 0xFF,
  };
  uint8_t image[AW862XX_RAM_HEADER_LEN + sizeof(s_sine_cycle)] = {
    AW862XX_RAM_HEADER_VERSION, start >> 8, start & 0xFF, end >> 8, end & 0xFF,
  };
  int ret;

  prv_scale_cycle(&image[AW862XX_RAM_HEADER_LEN], CONFIG_VIBE_AW86225_RATED_VOLTAGE_MV);

  ret = prv_update_bits(dev, AW862XX_REG_RTPCFG1, AW862XX_RTPCFG1_ADDRH_MASK,
                        PBL_FIELD_PREP(AW862XX_RTPCFG1_ADDRH_MASK, base >> 8));
  if (ret != 0) {
    return ret;
  }

  ret = prv_write_register(dev, AW862XX_REG_RTPCFG2, base & 0xFF);
  if (ret != 0) {
    return ret;
  }

  ret = prv_write_register_block(dev, AW862XX_REG_RTPCFG3, fifo, sizeof(fifo));
  if (ret != 0) {
    return ret;
  }

  ret = prv_write_register_block(dev, AW862XX_REG_RAMADDRH, addr, sizeof(addr));
  if (ret != 0) {
    return ret;
  }

  return prv_write_register_block(dev, AW862XX_REG_RAMDATA, image, sizeof(image));
}

static int prv_load_ram_image(const struct pbl_aw86225 *dev) {
  int ret;
  int ret_raminit;

  (void)prv_stop_playback(dev);

  ret = prv_set_raminit(dev, true);
  if (ret != 0) {
    return ret;
  }

  ret = prv_write_ram_image(dev);
  ret_raminit = prv_set_raminit(dev, false);
  if (ret != 0) {
    return ret;
  }

  return ret_raminit;
}

//! GAIN_CHANGEABLE lets PLAYCFG2 writes take effect mid-playback; otherwise
//! the gain is latched at GO.
static int prv_config_ram_playback(const struct pbl_aw86225 *dev) {
  int ret;

  ret =
      prv_update_bits(dev, AW862XX_REG_SYSCTRL2, AW862XX_SYSCTRL2_WAVDAT_MODE_MASK,
                      PBL_FIELD_PREP(AW862XX_SYSCTRL2_WAVDAT_MODE_MASK, AW862XX_SYSCTRL2_RATE_12K));
  if (ret != 0) {
    return ret;
  }

  ret = prv_update_bits(dev, AW862XX_REG_SYSCTRL7, AW862XX_SYSCTRL7_GAIN_CHANGEABLE,
                        AW862XX_SYSCTRL7_GAIN_CHANGEABLE);
  if (ret != 0) {
    return ret;
  }

  ret = prv_update_bits(dev, AW862XX_REG_PLAYCFG3,
                        AW862XX_PLAYCFG3_BRK_EN | AW862XX_PLAYCFG3_PLAY_MODE_MASK,
                        AW862XX_PLAYCFG3_BRK_EN | PBL_FIELD_PREP(AW862XX_PLAYCFG3_PLAY_MODE_MASK,
                                                                 AW862XX_PLAYCFG3_PLAY_MODE_RAM));
  if (ret != 0) {
    return ret;
  }

  const uint8_t regs[][2] = {
    {AW862XX_REG_WAVCFG1, AW862XX_RAM_WAVEFORM},
    {AW862XX_REG_WAVCFG2, AW862XX_WAVCFG_END},
    {AW862XX_REG_WAVCFG9,
     PBL_FIELD_PREP(AW862XX_WAVCFG9_SEQ1LOOP_MASK, AW862XX_WAVCFG9_LOOP_INFINITE)},
    {AW862XX_REG_PLAYCFG2, prv_gain_for_strength(dev, dev->state->strength)},
  };

  for (size_t i = 0; i < ARRAY_LENGTH(regs); i++) {
    ret = prv_write_register(dev, regs[i][0], regs[i][1]);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

static int prv_set_trim(const struct pbl_aw86225 *dev, uint8_t trim) {
  return prv_update_bits(dev, AW862XX_REG_TRIMCFG3, AW862XX_TRIMCFG3_TRIM_LRA_MASK,
                         PBL_FIELD_PREP(AW862XX_TRIMCFG3_TRIM_LRA_MASK, trim));
}

static int prv_start_f0_detection(const struct pbl_aw86225 *dev) {
  int ret;

  ret = prv_update_bits(dev, AW862XX_REG_PLAYCFG3,
                        AW862XX_PLAYCFG3_BRK_EN | AW862XX_PLAYCFG3_PLAY_MODE_MASK,
                        AW862XX_PLAYCFG3_BRK_EN | PBL_FIELD_PREP(AW862XX_PLAYCFG3_PLAY_MODE_MASK,
                                                                 AW862XX_PLAYCFG3_PLAY_MODE_CONT));
  if (ret != 0) {
    return ret;
  }

  ret = prv_update_bits(dev, AW862XX_REG_CONTCFG6,
                        AW862XX_CONTCFG6_TRACK_EN | AW862XX_CONTCFG6_DRV1_LVL_MASK,
                        AW862XX_CONTCFG6_TRACK_EN | PBL_FIELD_PREP(AW862XX_CONTCFG6_DRV1_LVL_MASK,
                                                                   AW862XX_CONTCFG7_FULL_SCALE));
  if (ret != 0) {
    return ret;
  }

  const uint8_t regs[][2] = {
    {AW862XX_REG_CONTCFG1,
     AW862XX_CONTCFG1_EDGE_FREQ_NONE | AW862XX_CONTCFG1_SIN_MODE_COS | AW862XX_CONTCFG1_EN_F0_DET},
    {AW862XX_REG_CONTCFG7, AW862XX_CONTCFG7_FULL_SCALE},
    {AW862XX_REG_CONTCFG2, AW862XX_CONTCFG2_CONF_F0},
    {AW862XX_REG_CONTCFG8, AW862XX_CONTCFG8_F0_DET_DRV1_TIME},
    {AW862XX_REG_CONTCFG9, AW862XX_CONTCFG9_F0_DET_DRV2_TIME},
    {AW862XX_REG_CONTCFG10, AW862XX_CONTCFG10_BRK_TIME},
    {AW862XX_REG_CONTCFG11, AW862XX_CONTCFG11_TRACK_MARGIN},
    {AW862XX_REG_CONTCFG3, AW862XX_CONTCFG3_F0_DET_DRV_WIDTH},
    {AW862XX_REG_PLAYCFG4, AW862XX_PLAYCFG4_GO},
  };

  for (size_t i = 0; i < ARRAY_LENGTH(regs); i++) {
    ret = prv_write_register(dev, regs[i][0], regs[i][1]);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

static int prv_read_f0_period(const struct pbl_aw86225 *dev, uint8_t reg_hi, uint16_t *period) {
  uint8_t hi;
  uint8_t lo;
  int ret;

  ret = prv_read_register(dev, reg_hi, &hi);
  if (ret != 0) {
    return ret;
  }

  ret = prv_read_register(dev, reg_hi + 1U, &lo);
  if (ret != 0) {
    return ret;
  }

  *period = ((uint16_t)hi << 8) | lo;

  return 0;
}

static int prv_measure_f0(const struct pbl_aw86225 *dev, int *f0) {
  uint16_t period;
  int ret;

  ret = prv_start_f0_detection(dev);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(AW862XX_F0_DET_STANDBY_POLL_MS * 2);

  ret = prv_wait_for_standby(dev, AW862XX_F0_DET_STANDBY_RETRIES, AW862XX_F0_DET_STANDBY_POLL_MS);
  if (ret != 0) {
    PBL_LOG_ERR("F0 detection did not reach standby (%d)", ret);
    (void)prv_write_register(dev, AW862XX_REG_PLAYCFG4, AW862XX_PLAYCFG4_STOP);
  }

  ret = prv_read_f0_period(dev, AW862XX_REG_CONTRD14, &period);
  if (ret == 0 && period == 0U) {
    ret = prv_read_f0_period(dev, AW862XX_REG_CONTRD16, &period);
  }
  if (ret == 0 && period == 0U) {
    ret = -EIO;
  }
  if (ret != 0) {
    PBL_LOG_ERR("F0 readback failed (%d)", ret);
    return ret;
  }

  *f0 = 384000 / period;

  return 0;
}

static int prv_detect_f0(const struct pbl_aw86225 *dev, int *f0) {
  int ret;
  int ret_restore;

  ret = prv_measure_f0(dev, f0);

  ret_restore = prv_update_bits(dev, AW862XX_REG_CONTCFG1, AW862XX_CONTCFG1_EN_F0_DET, 0U);
  if (ret_restore == 0) {
    ret_restore = prv_update_bits(dev, AW862XX_REG_PLAYCFG3, AW862XX_PLAYCFG3_BRK_EN, 0U);
  }

  if (ret != 0) {
    return ret;
  }

  return ret_restore;
}

//! TRIM_LRA is a 6-bit two's complement step count shifting the clock by
//! AW862XX_F0_CALI_LSB_PERMYRIAD per step; reject stored values outside the
//! LRA's F0 tolerance so a bad calibration cannot detune the drive.
static bool prv_trim_in_range(uint8_t trim) {
  int steps = (trim < 32) ? (int)trim : (int)trim - 64;
  int offset_hz =
      (int)CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ * steps * AW862XX_F0_CALI_LSB_PERMYRIAD / 10000;

  return (offset_hz >= -(int)CONFIG_VIBE_AW86225_LRA_FREQUENCY_TOLERANCE_HZ) &&
         (offset_hz <= (int)CONFIG_VIBE_AW86225_LRA_FREQUENCY_TOLERANCE_HZ);
}

static int prv_init(const struct pbl_vibe *vibe) {
  const struct pbl_aw86225 *dev = prv_get(vibe);
  uint8_t chip_id;
  int ret;

  ret = pbl_gpio_configure(&dev->reset, PBL_GPIO_OUTPUT_ACTIVE);
  if (ret != 0) {
    return ret;
  }

  pbl_sleep_ms(AW862XX_PWR_OFF_TIME);
  (void)pbl_gpio_set(&dev->reset, false);
  pbl_sleep_ms(AW862XX_PWR_ON_TIME);

  ret = prv_read_register(dev, AW862XX_REG_CHIPID, &chip_id);
  if (ret != 0) {
    PBL_LOG_ERR("Could not read the chip ID (%d)", ret);
    return -ENODEV;
  }

  ret = prv_load_ram_image(dev);
  if (ret != 0) {
    PBL_LOG_ERR("Could not load the waveform (%d)", ret);
    return ret;
  }

  if (dev->state->trim_valid) {
    ret = prv_set_trim(dev, dev->state->trim_lra);
    if (ret != 0) {
      PBL_LOG_ERR("Could not apply the calibration (%d)", ret);
      return ret;
    }
  }

  return 0;
}

static int prv_start(const struct pbl_vibe *vibe, int8_t strength) {
  const struct pbl_aw86225 *dev = prv_get(vibe);
  int ret;

  if (strength < 0) {
    strength = -strength;
  }
  if (strength > VIBE_STRENGTH_MAX) {
    strength = VIBE_STRENGTH_MAX;
  }

  dev->state->strength = strength;

  if (dev->state->playing) {
    return prv_write_register(dev, AW862XX_REG_PLAYCFG2,
                              prv_gain_for_strength(dev, dev->state->strength));
  }

  ret = prv_update_vbat(dev);
  if (ret != 0) {
    PBL_LOG_WRN("VBAT detection failed (%d)", ret);
  }

  ret = prv_config_ram_playback(dev);
  if (ret != 0) {
    PBL_LOG_ERR("Could not configure the playback (%d)", ret);
    return ret;
  }

  ret = prv_write_register(dev, AW862XX_REG_PLAYCFG4, AW862XX_PLAYCFG4_GO);
  if (ret != 0) {
    PBL_LOG_ERR("Could not start the playback (%d)", ret);
    return ret;
  }

  dev->state->playing = true;

  return 0;
}

static int prv_stop(const struct pbl_vibe *vibe) {
  const struct pbl_aw86225 *dev = prv_get(vibe);
  int ret;

  ret = prv_stop_playback(dev);
  if (ret != 0) {
    PBL_LOG_ERR("Could not confirm the playback stop (%d)", ret);
  }

  dev->state->playing = false;

  return ret;
}

// Refer to DG_AW862XX_Software_Design_Guide_CN_V1.1
static int prv_calibrate(const struct pbl_vibe *vibe, uint8_t *cali) {
  const struct pbl_aw86225 *dev = prv_get(vibe);
  const int f0_min =
      CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ - CONFIG_VIBE_AW86225_LRA_FREQUENCY_TOLERANCE_HZ;
  const int f0_max =
      CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ + CONFIG_VIBE_AW86225_LRA_FREQUENCY_TOLERANCE_HZ;
  int f0 = 0;
  int step;
  int ret;

  // Measure F0 with a neutral trim
  ret = prv_set_trim(dev, 0U);
  if (ret != 0) {
    return ret;
  }

  ret = prv_detect_f0(dev, &f0);
  if (ret != 0) {
    PBL_LOG_ERR("F0 detection failed (%d)", ret);
    return ret;
  }

  if (f0 < f0_min || f0 > f0_max) {
    PBL_LOG_ERR("F0 out of range (measured %d Hz, expected %d +/- %d Hz)", f0,
                CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ,
                CONFIG_VIBE_AW86225_LRA_FREQUENCY_TOLERANCE_HZ);
    return -ERANGE;
  }

  // TRIM_LRA is a 6-bit two's complement step of AW862XX_F0_CALI_LSB_PERMYRIAD (0.24%),
  // nearly +/- 7%: pick the step that brings F0 closest to the nominal frequency.
  step = 100000 * (f0 - (int)CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ) /
         ((int)CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ * AW862XX_F0_CALI_LSB_PERMYRIAD);
  if (step >= 0) {
    step = (step % 10 >= 5) ? (step / 10 + 1) : (step / 10);
  } else {
    step = (step % 10 <= -5) ? (step / 10 - 1) : (step / 10);
  }

  dev->state->trim_lra = PBL_FIELD_GET(AW862XX_TRIMCFG3_TRIM_LRA_MASK, (uint8_t)step);

  dev->state->trim_valid = true;

  ret = prv_set_trim(dev, dev->state->trim_lra);
  if (ret != 0) {
    return ret;
  }

  PBL_LOG_DBG("F0 measured %d Hz, trim 0x%02x", f0, dev->state->trim_lra);

  *cali = dev->state->trim_lra;

  return 0;
}

static int prv_set_calibration(const struct pbl_vibe *vibe, uint8_t cali) {
  const struct pbl_aw86225 *dev = prv_get(vibe);
  const uint8_t trim = PBL_FIELD_GET(AW862XX_TRIMCFG3_TRIM_LRA_MASK, cali);
  int ret;

  if (!prv_trim_in_range(trim)) {
    PBL_LOG_WRN("Ignoring calibration trim 0x%02x, out of the F0 tolerance", trim);
    return -EINVAL;
  }

  ret = prv_set_trim(dev, trim);
  if (ret != 0) {
    PBL_LOG_ERR("Could not apply the calibration (%d)", ret);
    return ret;
  }

  dev->state->trim_lra = trim;
  dev->state->trim_valid = true;
  PBL_LOG_DBG("Applied calibration trim 0x%02x", trim);

  return 0;
}

const struct pbl_vibe_ops pbl_aw86225_ops = {
  .init = prv_init,
  .start = prv_start,
  .stop = prv_stop,
  .calibrate = prv_calibrate,
  .set_calibration = prv_set_calibration,
};
