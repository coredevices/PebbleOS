/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/vibe.h"
#include "board/board.h"
#include "console/prompt.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "system/logging.h"
#include "system/passert.h"
#include "kernel/util/sleep.h"
#include <string.h>

PBL_LOG_MODULE_DEFINE(driver_vibe_aw86225, CONFIG_DRIVER_VIBE_LOG_LEVEL);

#define AW862XX_REG_SRST                                (0x00)
#define AW862XX_REG_PLAYCFG2                            (0x07)
#define AW862XX_REG_PLAYCFG3                            (0x08)
#define AW862XX_REG_PLAYCFG4                            (0x09)
#define AW862XX_REG_WAVCFG1                             (0x0A)
#define AW862XX_REG_WAVCFG2                             (0x0B)
#define AW862XX_REG_WAVCFG9                             (0x12)
#define AW862XX_REG_CONTCFG1                            (0x18)
#define AW862XX_REG_CONTCFG2                            (0x19)
#define AW862XX_REG_CONTCFG3                            (0x1A)
#define AW862XX_REG_CONTCFG6                            (0x1D)
#define AW862XX_REG_CONTCFG7                            (0x1E)
#define AW862XX_REG_CONTCFG8                            (0x1F)
#define AW862XX_REG_CONTCFG9                            (0x20)
#define AW862XX_REG_CONTCFG10                           (0x21)
#define AW862XX_REG_CONTCFG11                           (0x22)
#define AW862XX_REG_CONTRD14                            (0x25)
#define AW862XX_REG_CONTRD15                            (0x26)
#define AW862XX_REG_CONTRD16                            (0x27)
#define AW862XX_REG_CONTRD17                            (0x28)
#define AW862XX_REG_RTPCFG1                             (0x2D)
#define AW862XX_REG_RTPCFG2                             (0x2E)
#define AW862XX_REG_RTPCFG3                             (0x2F)
#define AW862XX_REG_GLBRD5                              (0x3F)
#define AW862XX_REG_RAMADDRH                            (0x40)
#define AW862XX_REG_RAMDATA                             (0x42)
#define AW862XX_REG_SYSCTRL1                            (0x43)
#define AW862XX_REG_SYSCTRL2                            (0x44)
#define AW862XX_REG_SYSCTRL7                            (0x49)
#define AW862XX_REG_DETCFG2                             (0x52)
#define AW862XX_REG_DET_VBAT                            (0x55)
#define AW862XX_REG_DET_LO                              (0x57)
#define AW862XX_REG_TRIMCFG3                            (0x5A)
#define AW862XX_REG_CHIPID                              (0x64)

#define AW862XX_BIT_PLAYCFG3_BRK_EN_MASK                (~(1<<2))
#define AW862XX_BIT_PLAYCFG3_BRK_ENABLE                 (1<<2)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_MASK             (~(3<<0))
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_RAM              (0<<0)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_CONT             (2<<0)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_STOP             (3<<0)

/* PLAYCFG4: reg 0x09 RW */
#define AW862XX_BIT_PLAYCFG4_STOP_ON                    (1<<1)
#define AW862XX_BIT_PLAYCFG4_GO_ON                      (1<<0)

#define AW862XX_F0_CALI_LSB_PERMYRIAD                   (24)
#define AW862XX_CONTCFG1_EDGE_FREQ_NONE                 (0x00)
#define AW862XX_CONTCFG1_SIN_MODE_COS                   (1<<0)
#define AW862XX_CONTCFG1_EN_F0_DET                      (1<<3)
#define AW862XX_CONTCFG2_CONF_F0                        (24000U / CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ)
#define AW862XX_CONTCFG3_F0_DET_DRV_WIDTH               (24000U / CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ - 8U - 8U - 15U)
#define AW862XX_CONTCFG7_FULL_SCALE                     (0x7FL)
#define AW862XX_CONTCFG8_F0_DET_DRV1_TIME               (0x04U)
#define AW862XX_CONTCFG9_F0_DET_DRV2_TIME               (0x14U)
#define AW862XX_CONTCFG10_BRK_TIME                      (0x08U)
#define AW862XX_CONTCFG11_TRACK_MARGIN                  (0x0FU)
#define AW862XX_CONTCFG6_TRACK_EN                       (1<<7)
#define AW862XX_RAM_BASE_ADDR                           (0x0800U)
#define AW862XX_RAM_HEADER_VERSION                      (0x01U)
#define AW862XX_RAM_HEADER_LEN                          (1U + 4U)
#define AW862XX_RAM_WAVEFORM                            (1U)
#define AW862XX_WAVCFG_END                              (0U)
#define AW862XX_WAVCFG9_LOOP_INFINITE                   (0x0FU)
#define AW862XX_PLAYCFG2_GAIN_UNITY                     (0x80U)
#define AW862XX_RMS_TO_PEAK_MILLI                       (1414U)
#define AW862XX_VBAT_REFER_MV                           (4200U)
#define AW862XX_VBAT_MIN_MV                             (3000U)
#define AW862XX_VBAT_MAX_MV                             (4500U)
#define AW862XX_VBAT_FULL_SCALE_MV                      (6100U)
#define AW862XX_VBAT_CODE_MAX                           (1024U)
#define AW862XX_PLAYCFG2_GAIN_LIMIT                     (AW862XX_PLAYCFG2_GAIN_UNITY * AW862XX_VBAT_REFER_MV / AW862XX_VBAT_MIN_MV)
#define AW862XX_RTPCFG1_ADDRH_MASK                      (~(0x0F<<0))
#define AW862XX_GLBRD5_STATE_MASK                       (0x0F)
#define AW862XX_GLBRD5_STATE_STANDBY                    (0x00)
#define AW862XX_TRIMCFG3_TRIM_LRA_MASK                  (~(0x3F))
#define AW862XX_SYSCTRL1_RAMINIT_MASK                   (~(1<<3))
#define AW862XX_SYSCTRL1_RAMINIT_ON                     (1<<3)
#define AW862XX_SYSCTRL1_RAMINIT_OFF                    (0<<3)
#define AW862XX_SYSCTRL2_STANDBY_MASK                   (~(1<<6))
#define AW862XX_SYSCTRL2_STANDBY_ON                     (1<<6)
#define AW862XX_SYSCTRL2_STANDBY_OFF                    (0<<6)
#define AW862XX_SYSCTRL2_WAVDAT_MODE_MASK               (~(3<<0))
#define AW862XX_SYSCTRL2_RATE_12K                       (2<<0)
#define AW862XX_SYSCTRL7_GAIN_BYPASS_MASK               (~(1<<6))
#define AW862XX_SYSCTRL7_GAIN_CHANGEABLE                (1<<6)
#define AW862XX_DETCFG2_VBAT_GO                         (1<<1)
#define AW862XX_DET_LO_VBAT_MASK                        (0x30)
#define AW862XX_DET_LO_VBAT_SHIFT                       (4)

#define AW862XX_PWR_OFF_TIME                            (2) /* ms */
#define AW862XX_PWR_ON_TIME                             (8) /* ms */
#define AW862XX_VBAT_DET_TIME                           (3) /* ms */
#define AW862XX_STOP_STANDBY_RETRIES                    (40)
#define AW862XX_STOP_STANDBY_POLL_MS                    (2)
#define AW862XX_F0_DET_STANDBY_RETRIES                  (200)
#define AW862XX_F0_DET_STANDBY_POLL_MS                  (10)
#define AW862XX_TRIM_LRA_INVALID                        (0xFF)

static bool s_initialized = false;
static int8_t s_target_strength = VIBE_STRENGTH_MAX;
static uint8_t s_trim_lra = AW862XX_TRIM_LRA_INVALID;
static uint16_t s_vbat_mv = AW862XX_VBAT_REFER_MV;
static bool s_playing = false;

#define AW862XX_OUTPUT_FULL_SCALE_MV \
  (AW862XX_VBAT_REFER_MV * CONFIG_VIBE_AW86225_OUTPUT_GAIN_PERCENT / 100U)

_Static_assert(CONFIG_VIBE_AW86225_RATED_VOLTAGE_MV * AW862XX_RMS_TO_PEAK_MILLI / 1000U <=
                   AW862XX_OUTPUT_FULL_SCALE_MV,
               "rated voltage exceeds the full-scale output");

//! One full-scale LRA cycle at 12 kS/s.
static const uint8_t s_sine_cycle[] = {
  0x00, 0x10, 0x20, 0x2f, 0x3d, 0x4b, 0x57, 0x62, 0x6b, 0x73,
  0x79, 0x7d, 0x7f, 0x7f, 0x7d, 0x79, 0x73, 0x6b, 0x62, 0x57,
  0x4b, 0x3d, 0x2f, 0x20, 0x10, 0x00, 0xf0, 0xe0, 0xd1, 0xc3,
  0xb5, 0xa9, 0x9e, 0x95, 0x8d, 0x87, 0x83, 0x81, 0x81, 0x83,
  0x87, 0x8d, 0x95, 0x9e, 0xa9, 0xb5, 0xc3, 0xd1, 0xe0, 0xf0,
};

static uint8_t s_ram_image[AW862XX_RAM_HEADER_LEN + sizeof(s_sine_cycle)];

static bool prv_read_register(uint8_t register_address, uint8_t* data) {
	i2c_use(I2C_AW86225);
	bool rv = i2c_read_register_block(I2C_AW86225, register_address, 1, data);
	i2c_release(I2C_AW86225);
	return rv;
}
  
static bool prv_write_register(uint8_t register_address, uint8_t datum) {
	i2c_use(I2C_AW86225);
	bool rv = i2c_write_register_block(I2C_AW86225, register_address, 1, &datum);
	i2c_release(I2C_AW86225);
	return rv;
}

static bool prv_write_register_block(uint8_t register_address, const uint8_t *data, size_t length) {
	i2c_use(I2C_AW86225);
	bool rv = i2c_write_register_block(I2C_AW86225, register_address, length, data);
	i2c_release(I2C_AW86225);
	return rv;
}

bool prv_modify_reg(uint8_t reg_addr, uint32_t mask, uint8_t reg_data)
{
	uint8_t reg_val = 0;
	uint8_t reg_mask = (uint8_t)mask;

	if (!prv_read_register(reg_addr, &reg_val)) {
		return false;
	}
	reg_val &= reg_mask;
	reg_val |= (reg_data & (~reg_mask));
	return prv_write_register(reg_addr, reg_val);
}

//! Start (flag=true) or stop (flag=false) playback. Returns true when the
//! command was written successfully; for stop, additionally requires the chip
//! to have reached standby. The waveform is an infinite hardware loop, so
//! a stop that silently fails leaves the motor running.
static bool prv_aw862xx_play_go(bool flag)
{
  uint8_t val;

  if (flag) {
    return prv_write_register(AW862XX_REG_PLAYCFG4, AW862XX_BIT_PLAYCFG4_GO_ON);
  }

  bool standby = false;
  bool ret = prv_modify_reg(AW862XX_REG_SYSCTRL1, AW862XX_SYSCTRL1_RAMINIT_MASK,
                            AW862XX_SYSCTRL1_RAMINIT_ON);
  ret &= prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_PLAY_MODE_MASK,
                        AW862XX_BIT_PLAYCFG3_PLAY_MODE_STOP);
  ret &= prv_write_register(AW862XX_REG_PLAYCFG4, AW862XX_BIT_PLAYCFG4_GO_ON);
  ret &= prv_modify_reg(AW862XX_REG_SYSCTRL1, AW862XX_SYSCTRL1_RAMINIT_MASK,
                        AW862XX_SYSCTRL1_RAMINIT_OFF);
  for (int i = 0; i < AW862XX_STOP_STANDBY_RETRIES; ++i) {
    if (!prv_read_register(AW862XX_REG_GLBRD5, &val)) {
      ret = false;
      break;
    }
    if ((val & AW862XX_GLBRD5_STATE_MASK) == AW862XX_GLBRD5_STATE_STANDBY) {
      standby = true;
      break;
    }
    psleep(AW862XX_STOP_STANDBY_POLL_MS);
  }
  if (!standby) {
    ret &= prv_modify_reg(AW862XX_REG_SYSCTRL2, AW862XX_SYSCTRL2_STANDBY_MASK,
                          AW862XX_SYSCTRL2_STANDBY_ON);
    ret &= prv_modify_reg(AW862XX_REG_SYSCTRL2, AW862XX_SYSCTRL2_STANDBY_MASK,
                          AW862XX_SYSCTRL2_STANDBY_OFF);
    if (ret && prv_read_register(AW862XX_REG_GLBRD5, &val)) {
      standby = (val & AW862XX_GLBRD5_STATE_MASK) == AW862XX_GLBRD5_STATE_STANDBY;
    }
  }
  return ret && standby;
}

//! Stop playback. Loudly reports a motor that could not be confirmed stopped.
static void prv_stop(void) {
  if (!prv_aw862xx_play_go(false)) {
    PBL_LOG_ERR("AW86225: failed to confirm playback stop");
  }
  s_playing = false;
}

static uint8_t prv_gain_for_strength(uint8_t strength) {
  uint32_t gain = (uint32_t)strength * AW862XX_PLAYCFG2_GAIN_UNITY * AW862XX_VBAT_REFER_MV /
                  (100U * s_vbat_mv);
  if (gain > AW862XX_PLAYCFG2_GAIN_LIMIT) {
    gain = AW862XX_PLAYCFG2_GAIN_LIMIT;
  }
  return gain;
}

//! Playback must be stopped.
static void prv_update_vbat(void) {
  uint8_t hi = 0;
  uint8_t lo = 0;
  bool ret = prv_modify_reg(AW862XX_REG_SYSCTRL1, AW862XX_SYSCTRL1_RAMINIT_MASK,
                            AW862XX_SYSCTRL1_RAMINIT_ON);
  ret &= prv_modify_reg(AW862XX_REG_DETCFG2, ~AW862XX_DETCFG2_VBAT_GO, AW862XX_DETCFG2_VBAT_GO);
  psleep(AW862XX_VBAT_DET_TIME);
  ret &= prv_read_register(AW862XX_REG_DET_VBAT, &hi);
  ret &= prv_read_register(AW862XX_REG_DET_LO, &lo);
  ret &= prv_modify_reg(AW862XX_REG_SYSCTRL1, AW862XX_SYSCTRL1_RAMINIT_MASK,
                        AW862XX_SYSCTRL1_RAMINIT_OFF);
  if (!ret) {
    PBL_LOG_WRN("AW86225: VBAT detect failed");
    return;
  }

  uint32_t code = ((uint32_t)hi << 2) |
                  ((lo & AW862XX_DET_LO_VBAT_MASK) >> AW862XX_DET_LO_VBAT_SHIFT);
  uint32_t vbat_mv = code * AW862XX_VBAT_FULL_SCALE_MV / AW862XX_VBAT_CODE_MAX;
  if (vbat_mv < AW862XX_VBAT_MIN_MV) {
    vbat_mv = AW862XX_VBAT_MIN_MV;
  } else if (vbat_mv > AW862XX_VBAT_MAX_MV) {
    vbat_mv = AW862XX_VBAT_MAX_MV;
  }
  s_vbat_mv = vbat_mv;
  PBL_LOG_DBG("AW86225: VBAT %u mV, full-strength gain %u", (unsigned)vbat_mv,
              prv_gain_for_strength(VIBE_STRENGTH_MAX));
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

static void prv_build_ram_image(void) {
  const uint16_t start = AW862XX_RAM_BASE_ADDR + AW862XX_RAM_HEADER_LEN;
  const uint16_t end = start + sizeof(s_sine_cycle) - 1;
  uint8_t *p = s_ram_image;

  *p++ = AW862XX_RAM_HEADER_VERSION;
  *p++ = start >> 8;
  *p++ = start & 0xFF;
  *p++ = end >> 8;
  *p++ = end & 0xFF;
  prv_scale_cycle(p, CONFIG_VIBE_AW86225_RATED_VOLTAGE_MV);
}

static bool prv_load_ram_image(void) {
  const uint16_t base = AW862XX_RAM_BASE_ADDR;
  uint8_t addr[] = { base >> 8, base & 0xFF };
  uint8_t fifo[] = {
    (((base >> 1) >> 4) & 0xF0) | (((base - (base >> 2)) >> 8) & 0x0F),
    (base >> 1) & 0xFF,
    (base - (base >> 2)) & 0xFF,
  };

  prv_aw862xx_play_go(false);
  bool ret = prv_modify_reg(AW862XX_REG_SYSCTRL1, AW862XX_SYSCTRL1_RAMINIT_MASK,
                            AW862XX_SYSCTRL1_RAMINIT_ON);
  ret &= prv_modify_reg(AW862XX_REG_RTPCFG1, AW862XX_RTPCFG1_ADDRH_MASK, base >> 8);
  ret &= prv_write_register(AW862XX_REG_RTPCFG2, base & 0xFF);
  ret &= prv_write_register_block(AW862XX_REG_RTPCFG3, fifo, sizeof(fifo));
  ret &= prv_write_register_block(AW862XX_REG_RAMADDRH, addr, sizeof(addr));
  ret &= prv_write_register_block(AW862XX_REG_RAMDATA, s_ram_image, sizeof(s_ram_image));
  ret &= prv_modify_reg(AW862XX_REG_SYSCTRL1, AW862XX_SYSCTRL1_RAMINIT_MASK,
                        AW862XX_SYSCTRL1_RAMINIT_OFF);
  return ret;
}

//! GAIN_CHANGEABLE lets PLAYCFG2 writes take effect mid-playback; otherwise
//! the gain is latched at GO.
static bool prv_config_ram_playback(void) {
  bool ret = prv_modify_reg(AW862XX_REG_SYSCTRL2, AW862XX_SYSCTRL2_WAVDAT_MODE_MASK,
                            AW862XX_SYSCTRL2_RATE_12K);
  ret &= prv_modify_reg(AW862XX_REG_SYSCTRL7, AW862XX_SYSCTRL7_GAIN_BYPASS_MASK,
                        AW862XX_SYSCTRL7_GAIN_CHANGEABLE);
  ret &= prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_BRK_EN_MASK,
                        AW862XX_BIT_PLAYCFG3_BRK_ENABLE);
  ret &= prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_PLAY_MODE_MASK,
                        AW862XX_BIT_PLAYCFG3_PLAY_MODE_RAM);
  ret &= prv_write_register(AW862XX_REG_WAVCFG1, AW862XX_RAM_WAVEFORM);
  ret &= prv_write_register(AW862XX_REG_WAVCFG2, AW862XX_WAVCFG_END);
  ret &= prv_write_register(AW862XX_REG_WAVCFG9, AW862XX_WAVCFG9_LOOP_INFINITE << 4);
  ret &= prv_write_register(AW862XX_REG_PLAYCFG2, prv_gain_for_strength(s_target_strength));
  return ret;
}

static int prv_f0_detection(void)
{
  int f0 = 0;
  uint8_t reg_val = 0;
  uint16_t f0_reg = 0;
  uint16_t cont_f0_reg = 0;
  bool standby = false;

  prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_PLAY_MODE_MASK,
                 AW862XX_BIT_PLAYCFG3_PLAY_MODE_CONT);
  prv_write_register(AW862XX_REG_CONTCFG1,
                     AW862XX_CONTCFG1_EDGE_FREQ_NONE | AW862XX_CONTCFG1_SIN_MODE_COS |
                         AW862XX_CONTCFG1_EN_F0_DET);
  prv_modify_reg(AW862XX_REG_CONTCFG6, ~AW862XX_CONTCFG6_TRACK_EN, AW862XX_CONTCFG6_TRACK_EN);
  prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_BRK_EN_MASK,
                 AW862XX_BIT_PLAYCFG3_BRK_ENABLE);
  prv_modify_reg(AW862XX_REG_CONTCFG6, ~AW862XX_CONTCFG7_FULL_SCALE,
                 AW862XX_CONTCFG7_FULL_SCALE);
  prv_write_register(AW862XX_REG_CONTCFG7, AW862XX_CONTCFG7_FULL_SCALE);
  prv_write_register(AW862XX_REG_CONTCFG2, AW862XX_CONTCFG2_CONF_F0);
  prv_write_register(AW862XX_REG_CONTCFG8, AW862XX_CONTCFG8_F0_DET_DRV1_TIME);
  prv_write_register(AW862XX_REG_CONTCFG9, AW862XX_CONTCFG9_F0_DET_DRV2_TIME);
  prv_write_register(AW862XX_REG_CONTCFG10, AW862XX_CONTCFG10_BRK_TIME);
  prv_write_register(AW862XX_REG_CONTCFG11, AW862XX_CONTCFG11_TRACK_MARGIN);
  prv_write_register(AW862XX_REG_CONTCFG3, AW862XX_CONTCFG3_F0_DET_DRV_WIDTH);

  prv_write_register(AW862XX_REG_PLAYCFG4, AW862XX_BIT_PLAYCFG4_GO_ON);
  psleep(AW862XX_F0_DET_STANDBY_POLL_MS * 2);

  for (int i = 0; i < AW862XX_F0_DET_STANDBY_RETRIES; ++i) {
    if (!prv_read_register(AW862XX_REG_GLBRD5, &reg_val)) {
      break;
    }

    if ((reg_val & AW862XX_GLBRD5_STATE_MASK) == AW862XX_GLBRD5_STATE_STANDBY) {
      standby = true;
      break;
    }

    psleep(AW862XX_F0_DET_STANDBY_POLL_MS);
  }

  if (!standby) {
    PBL_LOG_ERR("AW86225: F0 detect did not reach standby");
    prv_write_register(AW862XX_REG_PLAYCFG4, AW862XX_BIT_PLAYCFG4_STOP_ON);
  }

  bool ret = prv_read_register(AW862XX_REG_CONTRD14, &reg_val);
  f0_reg = reg_val << 8;
  ret &= prv_read_register(AW862XX_REG_CONTRD15, &reg_val);
  f0_reg |= reg_val;
  if (ret && f0_reg == 0) {
    ret = prv_read_register(AW862XX_REG_CONTRD16, &reg_val);
    cont_f0_reg = reg_val << 8;
    ret &= prv_read_register(AW862XX_REG_CONTRD17, &reg_val);
    cont_f0_reg |= reg_val;
    f0_reg = cont_f0_reg;
  }
  if (!ret || f0_reg == 0) {
    PBL_LOG_ERR("AW86225: F0 readback failed (i2c=%d, det=0x%04x, cont=0x%04x)", ret,
                f0_reg, cont_f0_reg);
    prv_modify_reg(AW862XX_REG_CONTCFG1, ~AW862XX_CONTCFG1_EN_F0_DET, 0);
    prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_BRK_EN_MASK, 0);
    return -1;
  }
  f0 = 384000 / f0_reg;

  prv_modify_reg(AW862XX_REG_CONTCFG1, ~AW862XX_CONTCFG1_EN_F0_DET, 0);
  prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_BRK_EN_MASK, 0);

  return f0;
}

void vibe_init(void) {
  gpio_output_init(&BOARD_CONFIG_VIBE.ctl, GPIO_OType_PP);

  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, true);
  psleep(AW862XX_PWR_OFF_TIME);
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, false);
  psleep(AW862XX_PWR_ON_TIME);

  uint8_t chip_id;
  bool ret = prv_read_register(AW862XX_REG_CHIPID, &chip_id);
  if (!ret) {
    PBL_LOG_ERR("AW86225: chip ID read failed (I2C error)");
    return;
  }

  prv_build_ram_image();
  ret &= prv_load_ram_image();
  if (s_trim_lra != AW862XX_TRIM_LRA_INVALID) {
    ret &= prv_modify_reg(AW862XX_REG_TRIMCFG3, AW862XX_TRIMCFG3_TRIM_LRA_MASK, s_trim_lra);
  }

  if (!ret) {
    PBL_LOG_ERR("AW86225: register configuration failed");
    return;
  }

  s_initialized = true;
}

void vibe_set_strength(int8_t strength) {
  if (strength < 0) {
    strength = -strength;
  }
  if (strength > VIBE_STRENGTH_MAX) {
    strength = VIBE_STRENGTH_MAX;
  }
  s_target_strength = strength;

  if (!s_initialized) {
    return;
  }

  if (!prv_write_register(AW862XX_REG_PLAYCFG2, prv_gain_for_strength(strength))) {
    PBL_LOG_ERR("AW86225: strength write failed");
  }
}

void vibe_ctl(bool on) {
  if (!s_initialized) {
    return;
  }

  if (on) {
    if (s_playing) {
      return;
    }
    prv_update_vbat();
    if (!prv_config_ram_playback()) {
      PBL_LOG_ERR("AW86225: playback configuration failed");
      return;
    }
    if (!prv_aw862xx_play_go(true)) {
      PBL_LOG_ERR("AW86225: playback start failed");
      return;
    }
    s_playing = true;
  } else {
    prv_stop();
  }
}

void vibe_force_off(void) {
  if (!s_initialized) {
    return;
  }

  prv_stop();
}

int8_t vibe_get_braking_strength(void) {
  return VIBE_STRENGTH_OFF;
}

// Refer to DG_AW862XX_Software_Design_Guide_CN_V1.1
status_t vibe_calibrate(void) {
  char f0_cali_lra = 0;
  int f0_cali_step = 0;
  int f0_cali_min = 0;
  int f0_cali_max = 0;
  int f0;
  
  if (!s_initialized) {
    return E_INVALID_OPERATION;
  }

  // Measure F0 with a neutral trim.
  prv_modify_reg(AW862XX_REG_TRIMCFG3, AW862XX_TRIMCFG3_TRIM_LRA_MASK, 0);

  f0 = prv_f0_detection();
  if (f0 < 0) {
    PBL_LOG_ERR("AW86225: F0 detection failed");
    return E_ERROR;
  }

  /**
   * TRIM_LRA[0:5] is used to calibration the f0 frequency.
   *
   * The TRIM_LRA LSB is 0.24%, up to 31, down to -32, calibration range
   * is nearly +/- 7%.
   *
   * Below code calibrate the f0 to match f0_pre as possible.
   */
  f0_cali_min = CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ - CONFIG_VIBE_AW86225_LRA_FREQUENCY_TOLERANCE_HZ;
  f0_cali_max = CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ + CONFIG_VIBE_AW86225_LRA_FREQUENCY_TOLERANCE_HZ;
  if (f0 < f0_cali_min || f0 > f0_cali_max) {
    PBL_LOG_ERR("AW86225: F0 out of range (measured %d Hz, expected %d +/- %d Hz)", f0,
                CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ, CONFIG_VIBE_AW86225_LRA_FREQUENCY_TOLERANCE_HZ);
    return E_ERROR;
  }

  f0_cali_step = 100000 * ((int)f0 - (int)CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ) /
                 ((int)CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ * AW862XX_F0_CALI_LSB_PERMYRIAD);
  if (f0_cali_step >= 0) {
    if (f0_cali_step % 10 >= 5) {
      f0_cali_step = 32 + (f0_cali_step / 10 + 1);
    } else {
      f0_cali_step = 32 + f0_cali_step / 10;
    }

  } else {
    if (f0_cali_step % 10 <= -5) {
      f0_cali_step = 32 + (f0_cali_step / 10 - 1);
    } else {
      f0_cali_step = 32 + f0_cali_step / 10;
    }
  }

  if (f0_cali_step > 31) {
    f0_cali_lra = (char)f0_cali_step - 32;
  } else {
    f0_cali_lra = (char)f0_cali_step + 32;
  }

  s_trim_lra = f0_cali_lra & 0x3F;
  prv_modify_reg(AW862XX_REG_TRIMCFG3, AW862XX_TRIMCFG3_TRIM_LRA_MASK, s_trim_lra);
  PBL_LOG_DBG("AW86225: F0 cali measured %d Hz, trim=0x%02x", f0, s_trim_lra);

  return S_SUCCESS;
}

uint8_t vibe_get_calibration(void) {
  return s_trim_lra;
}

//! TRIM_LRA is a 6-bit two's complement step count shifting the clock by
//! AW862XX_F0_CALI_LSB_PERMYRIAD per step; reject stored values outside the
//! LRA's F0 tolerance so a bad calibration cannot detune the drive.
static bool prv_trim_in_range(uint8_t trim) {
  int steps = (trim < 32) ? (int)trim : (int)trim - 64;
  int offset_hz = (int)CONFIG_VIBE_AW86225_LRA_FREQUENCY_HZ * steps * AW862XX_F0_CALI_LSB_PERMYRIAD / 10000;
  return (offset_hz >= -(int)CONFIG_VIBE_AW86225_LRA_FREQUENCY_TOLERANCE_HZ) &&
         (offset_hz <= (int)CONFIG_VIBE_AW86225_LRA_FREQUENCY_TOLERANCE_HZ);
}

void vibe_apply_calibration(uint8_t cali) {
  if (!s_initialized) {
    return;
  }

  if (!prv_trim_in_range(cali & 0x3F)) {
    PBL_LOG_WRN("AW86225: ignoring stored calibration trim=0x%02x (out of F0 tolerance)",
                cali & 0x3F);
    return;
  }

  s_trim_lra = cali & 0x3F;
  if (!prv_modify_reg(AW862XX_REG_TRIMCFG3, AW862XX_TRIMCFG3_TRIM_LRA_MASK, s_trim_lra)) {
    PBL_LOG_ERR("AW86225: failed to apply stored calibration");
    return;
  }
  PBL_LOG_DBG("AW86225: applied stored calibration trim=0x%02x", s_trim_lra);
}

void command_vibe_ctl(const char *arg) {
  if (!strcmp(arg, "cal")) {
    status_t rc = vibe_calibrate();
    if (rc != S_SUCCESS) {
      prompt_send_response("F0 cali fail");
    } else {
      prompt_send_response("F0 cali success");
    }

    return;
  }
  int strength = atoi(arg);

  const bool out_of_bounds = ((strength < 0) || (strength > VIBE_STRENGTH_MAX));
  const bool not_a_number = (strength == 0 && arg[0] != '0');
  if (out_of_bounds || not_a_number) {
    prompt_send_response("Invalid argument");
    return;
  }

  vibe_set_strength(strength);

  const bool turn_on = strength != 0;
  vibe_ctl(turn_on);
  prompt_send_response("OK");
}
