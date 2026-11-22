/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/adc.h>
#include <pbl/drivers/nvmem.h>
#include <pbl/drivers/temp.h>
#include <pbl/util/misc.h>

// Internal channels of ADC3
#define CHANNEL_VSENSE  18U
#define CHANNEL_VREFINT 19U

// The calibration temperatures, in millidegrees: the second is 130 degrees from revision V on
#define CAL1_MC 30000
#define CAL2_MC 130000

struct pbl_temp_stm32h7_adc_state {
  // The sensor at the two temperatures and VREFINT, as read at 3.3 V
  uint16_t ts_cal1;
  uint16_t ts_cal2;
  uint16_t vrefint_cal;
};

struct pbl_temp_stm32h7_adc {
  struct pbl_temp temp;
  const struct pbl_adc *adc;
  struct pbl_nvmem_cell vrefint;
  struct pbl_nvmem_cell ts_cal1;
  struct pbl_nvmem_cell ts_cal2;
  struct pbl_temp_stm32h7_adc_state *state;
};

static const struct pbl_temp_stm32h7_adc *prv_temp(const struct pbl_temp *temp) {
  return PBL_CONTAINER_OF(temp, const struct pbl_temp_stm32h7_adc, temp);
}

static int prv_init(const struct pbl_temp *dev) {
  const struct pbl_temp_stm32h7_adc *temp = prv_temp(dev);
  struct pbl_temp_stm32h7_adc_state *st = temp->state;
  int ret;

  ret = pbl_device_init(&temp->adc->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_nvmem_cell_read(&temp->ts_cal1, &st->ts_cal1, sizeof(st->ts_cal1));
  if (ret == 0) {
    ret = pbl_nvmem_cell_read(&temp->ts_cal2, &st->ts_cal2, sizeof(st->ts_cal2));
  }
  if (ret == 0) {
    ret = pbl_nvmem_cell_read(&temp->vrefint, &st->vrefint_cal, sizeof(st->vrefint_cal));
  }
  if (ret != 0) {
    return ret;
  }

  return (st->ts_cal2 > st->ts_cal1 && st->vrefint_cal != 0U) ? 0 : -EINVAL;
}

// The sensor as it would read at 3.3 V: its ratio to VREFINT, scaled by VREFINT at 3.3 V,
// whatever VREF+ is, then a line through the two calibration points
static int prv_read(const struct pbl_temp *dev, int32_t *millicelsius) {
  const struct pbl_temp_stm32h7_adc *temp = prv_temp(dev);
  const struct pbl_temp_stm32h7_adc_state *st = temp->state;
  const struct pbl_adc_channel vsense = PBL_ADC_CHANNEL(temp->adc, CHANNEL_VSENSE);
  const struct pbl_adc_channel vrefint = PBL_ADC_CHANNEL(temp->adc, CHANNEL_VREFINT);
  int32_t sense_uv;
  int32_t ref_uv;
  int64_t raw;
  int ret;

  ret = pbl_adc_read(&vrefint, &ref_uv);
  if (ret == 0) {
    ret = pbl_adc_read(&vsense, &sense_uv);
  }
  if (ret != 0) {
    return ret;
  }

  if (ref_uv <= 0) {
    return -EIO;
  }

  raw = (int64_t)sense_uv * st->vrefint_cal / ref_uv;
  *millicelsius =
      (int32_t)(CAL1_MC + (raw - st->ts_cal1) * (CAL2_MC - CAL1_MC) / (st->ts_cal2 - st->ts_cal1));

  return 0;
}

static const struct pbl_temp_ops s_ops = {
  .init = prv_init,
  .read = prv_read,
};

#define PBL_TEMP_STM32H7_ADC_DEFINE(sym, _name, _adc, _vrefint, _ts_cal1, _ts_cal2)         \
  PBL_TEMP_STATE_DEFINE(sym);                                                               \
  static struct pbl_temp_stm32h7_adc_state sym##_adc_state;                                 \
  PBL_DEVICE_INSTANCE(sym, struct pbl_temp_stm32h7_adc, struct pbl_temp, temp) {            \
    .temp = PBL_TEMP_INIT(sym, _name, nullptr, &s_ops), .adc = (_adc), .vrefint = _vrefint, \
    .ts_cal1 = _ts_cal1, .ts_cal2 = _ts_cal2, .state = &sym##_adc_state,                    \
  }

#include PBL_DT_INSTANCES(st_stm32h7_adc_temp)
