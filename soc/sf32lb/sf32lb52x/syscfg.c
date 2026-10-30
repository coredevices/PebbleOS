/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscfg.h"

#include <stdint.h>
#include <string.h>

#include <bf0_hal.h>

// Factory test calibration, efuse bank 1
#define ATE_BIT_OFFSET 256U
#define ATE_SIZE       32U
#define ATE_SETTLE_US  10U

#define PID_D 4U

static uint8_t s_ate[ATE_SIZE];

void soc_sf32lb52_syscfg_init(void) {
  if (HAL_EFUSE_Init() != HAL_OK) {
    return;
  }

  HAL_Delay_us(0);
  HAL_Delay_us(ATE_SETTLE_US);

  if (HAL_EFUSE_Read(ATE_BIT_OFFSET, s_ate, sizeof(s_ate)) != (int32_t)sizeof(s_ate)) {
    memset(s_ate, 0, sizeof(s_ate));
    return;
  }

  HAL_PMU_LoadCalData();
}

// The PMU driver gets its trims through this; nothing uses the other records
int BSP_CONFIG_get(int type, uint8_t *buf, int length) {
  const uint8_t *d = s_ate;
  FACTORY_CFG_VBK_LDO_T *cfg = (FACTORY_CFG_VBK_LDO_T *)buf;
  const bool letter = SF32LB52X_LETTER_SERIES();

  // Byte 0 holds trims, so it is never 0 on a calibrated part
  if (type != FACTORY_CFG_ID_VBUCK || buf == nullptr || length < (int)sizeof(*cfg) || d[0] == 0U) {
    return 0;
  }

  if (letter && __HAL_SYSCFG_GET_PID() == PID_D) {
    cfg->buck_vos_trim = d[20] & 0x7U;
    cfg->buck_vos_polar = (d[20] & 0x8U) >> 3;
    cfg->hpsys_ldo_vout = (d[20] & 0xF0U) >> 4;
    cfg->lpsys_ldo_vout = d[21] & 0xFU;
    cfg->vret_trim = (d[21] & 0xF0U) >> 4;
    cfg->hpsys_ldo_vout2 = (d[28] & 0x78U) >> 3;
  } else {
    cfg->buck_vos_trim = d[0] & 0x7U;
    cfg->buck_vos_polar = (d[0] & 0x8U) >> 3;
    cfg->hpsys_ldo_vout = (d[0] & 0xF0U) >> 4;
    cfg->lpsys_ldo_vout = d[1] & 0xFU;
    cfg->vret_trim = (d[1] & 0xF0U) >> 4;
    cfg->hpsys_ldo_vout2 = (d[13] & 0xF0U) >> 4;
    cfg->lpsys_ldo_vout2 = d[14] & 0xFU;
  }

  if (!letter) {
    cfg->vdd33_ldo2_vout = (d[2] & 0xF0U) >> 4;
    cfg->vdd33_ldo3_vout = d[3] & 0xFU;
    cfg->aon_vos_trim = (d[3] & 0x70U) >> 4;
    cfg->aon_vos_polar = (d[3] & 0x80U) >> 7;
  }

  cfg->ldo18_vref_sel = d[2] & 0xFU;

  if (cfg->hpsys_ldo_vout == 0U || cfg->hpsys_ldo_vout2 == 0U) {
    return 0;
  }

  return length;
}
