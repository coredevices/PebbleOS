/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/vibe.h>
#include "board/board.h"
#include "console/prompt.h"
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/vibe/vibe_aw86225.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>
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
#define AW862XX_REG_WAVCFG3                             (0x0C)
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
#define AW862XX_REG_DETCFG2                             (0x52)
#define AW862XX_REG_DET_VBAT                            (0x55)
#define AW862XX_REG_DET_LO                              (0x57)
#define AW862XX_REG_TRIMCFG3                            (0x5A)
#define AW862XX_REG_CHIPID                              (0x64)

#define AW862XX_BIT_PLAYCFG3_BRK_EN_MASK                (~(1<<2))
#define AW862XX_BIT_PLAYCFG3_BRK_ENABLE                 (1<<2)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_MASK             (~(3<<0))
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_CONT             (2<<0)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_RAM              (0<<0)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_STOP             (3<<0)

/* PLAYCFG4: reg 0x09 RW */
#define AW862XX_BIT_PLAYCFG4_STOP_ON                    (1<<1)
#define AW862XX_BIT_PLAYCFG4_GO_ON                      (1<<0)

#define AW862XX_F0_CALI_LSB_PERMYRIAD                   (24)
#define AW862XX_CONTCFG1_EDGE_FREQ_NONE                 (0x00)
#define AW862XX_CONTCFG1_SIN_MODE_COS                   (1<<0)
#define AW862XX_CONTCFG1_EN_F0_DET                      (1<<3)
#define AW862XX_CONTCFG2_CONF_F0                        (24000U / s_drive_frequency_hz)
#define AW862XX_CONTCFG3_DRV_WIDTH                      (24000U / s_drive_frequency_hz - 8U - 8U - 15U)
#define AW862XX_CONTCFG3_F0_DET_DRV_WIDTH               (24000U / AW86225->lra_frequency_hz - 8U - 8U - 15U)
#define AW862XX_CONTCFG7_FULL_SCALE                     (0x7FL)
#define AW862XX_CONT_LVL_FULL_SCALE_MV                  (5000U)
#define AW862XX_CONT_HALF_CYCLES_PER_CYCLE              (2U)
#define AW862XX_CONTCFG8_DRV1_TIME                      (0x04U)
#define AW862XX_CONTCFG9_DRV2_TIME_MAX                  (0xFFU)
#define AW862XX_CONTCFG10_BRK_TIME                      (0x08U)
#define AW862XX_CONTCFG11_TRACK_MARGIN                  (0x0FU)
#define AW862XX_CONTCFG6_TRACK_EN                       (1<<7)
#define AW862XX_RAM_BASE_ADDR                           (0x0010U)
#define AW862XX_RAM_HEADER_VERSION                      (0x55U)
#define AW862XX_RAM_HEADER_LEN                          (1U + 2U * 4U)
#define AW862XX_RAM_WAVEFORM_OVERDRIVE                  (0x01U)
#define AW862XX_RAM_WAVEFORM_SUSTAIN                    (0x02U)
#define AW862XX_WAVCFG9_LOOP_INFINITE                   (0x0FU)
#define AW862XX_PLAYCFG2_GAIN_UNITY                     (0x80U)
#define AW862XX_VBAT_REFER_MV                           (4200U)
#define AW862XX_VBAT_MIN_MV                             (3000U)
#define AW862XX_VBAT_MAX_MV                             (4500U)
#define AW862XX_VBAT_FULL_SCALE_MV                      (6100U)
#define AW862XX_VBAT_CODE_MAX                           (1024U)
#define AW862XX_OVERDRIVE_PEAK_MV                       (CONFIG_VIBE_AW86225_OVERDRIVE_VOLTAGE_MV * 1414U / 1000U)
#define AW862XX_PLAYCFG2_GAIN_LIMIT                     (AW862XX_PLAYCFG2_GAIN_UNITY * AW862XX_VBAT_REFER_MV / AW862XX_VBAT_MIN_MV)
#define AW862XX_RTPCFG1_ADDRH_MASK                      (~(0x0F<<0))
#define AW862XX_GLBRD5_STATE_MASK                       (0x0F)
#define AW862XX_GLBRD5_STATE_STANDBY                    (0x00)
#define AW862XX_TRIMCFG3_TRIM_LRA_MASK                  (~(0x3F))
#define AW862XX_SYSCTRL1_RAMINIT_MASK                   (~(1<<3))
#define AW862XX_SYSCTRL1_RAMINIT_ON                     (1<<3)
#define AW862XX_SYSCTRL1_RAMINIT_OFF                    (0<<3)
#define AW862XX_SYSCTRL1_VBAT_MODE_MASK                 (~(1<<7))
#define AW862XX_SYSCTRL1_VBAT_MODE_EN                   (1<<7)
#define AW862XX_SYSCTRL2_STANDBY_MASK                   (~(1<<6))
#define AW862XX_SYSCTRL2_STANDBY_ON                     (1<<6)
#define AW862XX_SYSCTRL2_STANDBY_OFF                    (0<<6)
#define AW862XX_SYSCTRL2_WAVDAT_MODE_MASK               (~(3<<0))
#define AW862XX_SYSCTRL2_RATE_12K                       (2<<0)
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
static uint16_t s_drive_frequency_hz;
static uint8_t s_trim_lra = AW862XX_TRIM_LRA_INVALID;
#ifndef CONFIG_VIBE_AW86225_CONT_PLAYBACK
static uint16_t s_vbat_mv = AW862XX_VBAT_REFER_MV;
#endif
static bool s_playing = false;

_Static_assert(CONFIG_VIBE_AW86225_RATED_VOLTAGE_MV <= CONFIG_VIBE_AW86225_OVERDRIVE_VOLTAGE_MV,
               "overdrive voltage must not be below the rated voltage");

static const uint8_t s_sine_cycle[] = {
  0x00, 0x11, 0x22, 0x31, 0x40, 0x4e, 0x5a, 0x64, 0x6e, 0x74,
  0x7a, 0x7d, 0x7f, 0x7f, 0x7d, 0x7a, 0x74, 0x6e, 0x64, 0x5a,
  0x4e, 0x40, 0x31, 0x22, 0x11, 0x00, 0xef, 0xde, 0xcf, 0xc0,
  0xb2, 0xa6, 0x9c, 0x92, 0x8c, 0x86, 0x83, 0x81, 0x81, 0x83,
  0x86, 0x8c, 0x92, 0x9c, 0xa6, 0xb2, 0xc0, 0xcf, 0xde, 0xef,
};

static uint8_t s_ram_image[AW862XX_RAM_HEADER_LEN + 2 * sizeof(s_sine_cycle)];

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
//! to have reached standby. The RAM waveform is configured as an infinite
//! hardware loop, so a stop that silently fails leaves the motor running.
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

#ifndef CONFIG_VIBE_AW86225_CONT_PLAYBACK
static uint8_t prv_gain_for_strength(uint8_t strength) {
  uint32_t gain = (uint32_t)strength * AW862XX_OVERDRIVE_PEAK_MV * AW862XX_PLAYCFG2_GAIN_UNITY /
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
#endif

//! CONT mode drives a square wave for DRV_WIDTH of each half cycle, so the RMS
//! target maps to a peak of rms / sqrt(width / period).
static uint8_t prv_cont_level(uint32_t rms_mv, uint8_t strength) {
  uint32_t period = AW862XX_CONTCFG2_CONF_F0;
  uint32_t width = AW862XX_CONTCFG3_DRV_WIDTH;
  uint32_t peak_mv = (rms_mv * integer_sqrt(((int64_t)period << 20) / width)) >> 10;
  uint32_t level = (uint32_t)strength * peak_mv * AW862XX_PLAYCFG2_GAIN_UNITY /
                   (100U * AW862XX_CONT_LVL_FULL_SCALE_MV);
  if (level > AW862XX_CONTCFG7_FULL_SCALE) {
    level = AW862XX_CONTCFG7_FULL_SCALE;
  }
  return level;
}

static bool prv_write_cont_levels(uint8_t strength) {
  uint8_t drv1_level = prv_cont_level(CONFIG_VIBE_AW86225_OVERDRIVE_VOLTAGE_MV, strength);
  uint8_t drv2_level = prv_cont_level(CONFIG_VIBE_AW86225_RATED_VOLTAGE_MV, strength);
  bool ret = prv_write_register(AW862XX_REG_CONTCFG7, drv2_level);
  ret &= prv_write_register(AW862XX_REG_CONTCFG6, AW862XX_CONTCFG6_TRACK_EN | drv1_level);
  return ret;
}

static bool prv_config_cont_mode(uint8_t drv1_time, uint8_t drv2_time) {
  bool ret = prv_write_register(AW862XX_REG_CONTCFG1,
                                AW862XX_CONTCFG1_EDGE_FREQ_NONE |
                                    AW862XX_CONTCFG1_SIN_MODE_COS);
  ret &= prv_write_register(AW862XX_REG_CONTCFG2, AW862XX_CONTCFG2_CONF_F0);
  ret &= prv_write_register(AW862XX_REG_CONTCFG3, AW862XX_CONTCFG3_DRV_WIDTH);
  ret &= prv_write_cont_levels(s_target_strength);
  ret &= prv_write_register(AW862XX_REG_CONTCFG8, drv1_time);
  ret &= prv_write_register(AW862XX_REG_CONTCFG9, drv2_time);
  ret &= prv_write_register(AW862XX_REG_CONTCFG10, AW862XX_CONTCFG10_BRK_TIME);
  ret &= prv_write_register(AW862XX_REG_CONTCFG11, AW862XX_CONTCFG11_TRACK_MARGIN);
  ret &= prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_BRK_EN_MASK,
                        AW862XX_BIT_PLAYCFG3_BRK_ENABLE);
  ret &= prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_PLAY_MODE_MASK,
                        AW862XX_BIT_PLAYCFG3_PLAY_MODE_CONT);
  ret &= prv_modify_reg(AW862XX_REG_SYSCTRL1, AW862XX_SYSCTRL1_VBAT_MODE_MASK,
                        AW862XX_SYSCTRL1_VBAT_MODE_EN);
  return ret;
}

//! Waveform 1 is the full-scale sine used for overdrive, waveform 2 the same
//! cycle scaled down to the rated voltage for sustain.
static void prv_build_ram_image(void) {
  const uint16_t od_start = AW862XX_RAM_BASE_ADDR + AW862XX_RAM_HEADER_LEN;
  const uint16_t od_end = od_start + sizeof(s_sine_cycle) - 1;
  const uint16_t sus_start = od_end + 1;
  const uint16_t sus_end = sus_start + sizeof(s_sine_cycle) - 1;
  uint8_t *p = s_ram_image;

  *p++ = AW862XX_RAM_HEADER_VERSION;
  *p++ = od_start >> 8;
  *p++ = od_start & 0xFF;
  *p++ = od_end >> 8;
  *p++ = od_end & 0xFF;
  *p++ = sus_start >> 8;
  *p++ = sus_start & 0xFF;
  *p++ = sus_end >> 8;
  *p++ = sus_end & 0xFF;
  memcpy(p, s_sine_cycle, sizeof(s_sine_cycle));
  p += sizeof(s_sine_cycle);
  for (size_t i = 0; i < sizeof(s_sine_cycle); ++i) {
    int32_t sample = (int8_t)s_sine_cycle[i];
    sample = sample * CONFIG_VIBE_AW86225_RATED_VOLTAGE_MV / CONFIG_VIBE_AW86225_OVERDRIVE_VOLTAGE_MV;
    *p++ = (uint8_t)(int8_t)sample;
  }
}

static bool prv_load_ram_waveform(void) {
  uint8_t addr[] = { AW862XX_RAM_BASE_ADDR >> 8, AW862XX_RAM_BASE_ADDR & 0xFF };
  uint8_t fifo[] = {
    (((AW862XX_RAM_BASE_ADDR >> 1) >> 4) & 0xF0) |
        (((AW862XX_RAM_BASE_ADDR - (AW862XX_RAM_BASE_ADDR >> 2)) >> 8) & 0x0F),
    (AW862XX_RAM_BASE_ADDR >> 1) & 0xFF,
    (AW862XX_RAM_BASE_ADDR - (AW862XX_RAM_BASE_ADDR >> 2)) & 0xFF,
  };

  prv_aw862xx_play_go(false);
  bool ret = prv_modify_reg(AW862XX_REG_SYSCTRL1, AW862XX_SYSCTRL1_RAMINIT_MASK,
                            AW862XX_SYSCTRL1_RAMINIT_ON);
  ret &= prv_modify_reg(AW862XX_REG_RTPCFG1, AW862XX_RTPCFG1_ADDRH_MASK,
                        AW862XX_RAM_BASE_ADDR >> 8);
  ret &= prv_write_register(AW862XX_REG_RTPCFG2, AW862XX_RAM_BASE_ADDR & 0xFF);
  ret &= prv_write_register_block(AW862XX_REG_RTPCFG3, fifo, sizeof(fifo));
  ret &= prv_write_register_block(AW862XX_REG_RAMADDRH, addr, sizeof(addr));
  ret &= prv_write_register_block(AW862XX_REG_RAMDATA, s_ram_image, sizeof(s_ram_image));
  ret &= prv_modify_reg(AW862XX_REG_SYSCTRL1, AW862XX_SYSCTRL1_RAMINIT_MASK,
                        AW862XX_SYSCTRL1_RAMINIT_OFF);
  return ret;
}

#ifndef CONFIG_VIBE_AW86225_CONT_PLAYBACK
static bool prv_config_ram_loop_mode(void) {
  uint8_t gain = prv_gain_for_strength(s_target_strength);
  bool ret = prv_modify_reg(AW862XX_REG_SYSCTRL2, AW862XX_SYSCTRL2_WAVDAT_MODE_MASK,
                            AW862XX_SYSCTRL2_RATE_12K);
  ret &= prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_BRK_EN_MASK,
                        AW862XX_BIT_PLAYCFG3_BRK_ENABLE);
  ret &= prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_PLAY_MODE_MASK,
                        AW862XX_BIT_PLAYCFG3_PLAY_MODE_RAM);
  ret &= prv_write_register(AW862XX_REG_WAVCFG1, AW862XX_RAM_WAVEFORM_OVERDRIVE);
  ret &= prv_write_register(AW862XX_REG_WAVCFG2, AW862XX_RAM_WAVEFORM_SUSTAIN);
  ret &= prv_write_register(AW862XX_REG_WAVCFG3, 0);
  ret &= prv_write_register(AW862XX_REG_WAVCFG9,
                            ((CONFIG_VIBE_AW86225_OVERDRIVE_CYCLES - 1) << 4) |
                                AW862XX_WAVCFG9_LOOP_INFINITE);
  ret &= prv_write_register(AW862XX_REG_PLAYCFG2, gain);
  return ret;
}
#endif

static bool prv_config_playback(void) {
#ifdef CONFIG_VIBE_AW86225_CONT_PLAYBACK
  return prv_config_cont_mode(
      CONFIG_VIBE_AW86225_OVERDRIVE_CYCLES * AW862XX_CONT_HALF_CYCLES_PER_CYCLE,
      AW862XX_CONTCFG9_DRV2_TIME_MAX);
#else
  return prv_config_ram_loop_mode();
#endif
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
  prv_modify_reg(AW862XX_REG_CONTCFG1, ~AW862XX_CONTCFG1_EN_F0_DET,
                 AW862XX_CONTCFG1_EN_F0_DET);
  prv_modify_reg(AW862XX_REG_CONTCFG6, ~AW862XX_CONTCFG6_TRACK_EN, AW862XX_CONTCFG6_TRACK_EN);
  prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_BRK_EN_MASK,
                 AW862XX_BIT_PLAYCFG3_BRK_ENABLE);
  prv_modify_reg(AW862XX_REG_CONTCFG6, ~AW862XX_CONTCFG7_FULL_SCALE,
                 AW862XX_CONTCFG7_FULL_SCALE);
  prv_write_register(AW862XX_REG_CONTCFG7, AW862XX_CONTCFG7_FULL_SCALE);
  prv_write_register(AW862XX_REG_CONTCFG8, 0x04);
  prv_write_register(AW862XX_REG_CONTCFG9, 0x14);
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
  PBL_ASSERTN(AW86225->lra_frequency_hz > 0);
  PBL_ASSERTN(AW86225->lra_frequency_tolerance_hz > 0);
  if (s_drive_frequency_hz == 0) {
    s_drive_frequency_hz = AW86225->lra_frequency_hz;
  }

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
  ret &= prv_load_ram_waveform();
  if (s_trim_lra != AW862XX_TRIM_LRA_INVALID) {
    ret &= prv_modify_reg(AW862XX_REG_TRIMCFG3, AW862XX_TRIMCFG3_TRIM_LRA_MASK, s_trim_lra);
  }
  ret &= prv_config_playback();

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

#ifdef CONFIG_VIBE_AW86225_CONT_PLAYBACK
  bool ret = prv_write_cont_levels(strength);
#else
  bool ret = prv_write_register(AW862XX_REG_PLAYCFG2, prv_gain_for_strength(strength));
#endif
  if (!ret) {
    PBL_LOG_ERR("AW86225: strength write failed");
  }
}

void vibe_ctl(bool on) {
  if (!s_initialized) {
    return;
  }

  if (on) {
#ifndef CONFIG_VIBE_AW86225_CONT_PLAYBACK
    if (!s_playing) {
      prv_update_vbat();
    }
#endif
    if (!prv_config_playback()) {
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
  if (!s_initialized) {
    return 0;
  }

  uint8_t value;
  prv_read_register(AW862XX_REG_CONTCFG7, &value);
  uint8_t strength = value * 100UL/AW862XX_CONTCFG7_FULL_SCALE;
  return strength;
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
  f0_cali_min = AW86225->lra_frequency_hz - AW86225->lra_frequency_tolerance_hz;
  f0_cali_max = AW86225->lra_frequency_hz + AW86225->lra_frequency_tolerance_hz;
  if (f0 < f0_cali_min || f0 > f0_cali_max) {
    PBL_LOG_ERR("AW86225: F0 out of range (measured %d Hz, expected %d +/- %d Hz)", f0,
                AW86225->lra_frequency_hz, AW86225->lra_frequency_tolerance_hz);
    return E_ERROR;
  }

  f0_cali_step = 100000 * ((int)f0 - (int)AW86225->lra_frequency_hz) /
                 ((int)AW86225->lra_frequency_hz * AW862XX_F0_CALI_LSB_PERMYRIAD);
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
  s_drive_frequency_hz = f0;
  prv_config_cont_mode(AW862XX_CONTCFG8_DRV1_TIME, AW862XX_CONTCFG9_DRV2_TIME_MAX);
  PBL_LOG_DBG("AW86225: F0 cali measured %d Hz, trim=0x%02x", f0, s_trim_lra);

  return S_SUCCESS;
}

uint8_t vibe_get_calibration(void) {
  return s_trim_lra;
}

void vibe_apply_calibration(uint8_t cali) {
  if (!s_initialized) {
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
