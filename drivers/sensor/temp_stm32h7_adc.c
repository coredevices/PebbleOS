/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/adc.h>
#include <pbl/drivers/nvmem.h>
#include <pbl/drivers/sensor.h>
#include <pbl/kernel/time.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

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
  struct pbl_sensor sensor;
  const struct pbl_adc *adc;
  struct pbl_nvmem_cell vrefint;
  struct pbl_nvmem_cell ts_cal1;
  struct pbl_nvmem_cell ts_cal2;
  struct pbl_temp_stm32h7_adc_state *state;
};

static const struct pbl_sensor_chan s_chans[] = {
  PBL_SENSOR_CHAN(PBL_SENSOR_TEMP, 0, 0, PBL_SENSOR_FMT_S32_LE, 1, 1, 0),
};

static const struct pbl_temp_stm32h7_adc *prv_temp(const struct pbl_sensor *sensor) {
  return PBL_CONTAINER_OF(sensor, const struct pbl_temp_stm32h7_adc, sensor);
}

static int prv_init(const struct pbl_sensor *dev) {
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
static int prv_read(const struct pbl_sensor *dev, enum pbl_sensor_type type,
                    struct pbl_sensor_frame *frame) {
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
  pbl_put_le32(
      (uint32_t)(CAL1_MC + (raw - st->ts_cal1) * (CAL2_MC - CAL1_MC) / (st->ts_cal2 - st->ts_cal1)),
      frame->data);
  frame->chans = s_chans;
  frame->num_chans = PBL_ARRAY_SIZE(s_chans);
  frame->timestamp_us = pbl_uptime_us();

  return 0;
}

static const struct pbl_sensor_ops s_ops = {
  .init = prv_init,
  .read = prv_read,
};

#define PBL_TEMP_STM32H7_ADC_DEFINE(sym, _name, _adc, _vrefint, _ts_cal1, _ts_cal2)      \
  PBL_SENSOR_STATE_DEFINE(sym);                                                          \
  static struct pbl_temp_stm32h7_adc_state sym##_adc_state;                              \
  PBL_DEVICE_INSTANCE(sym, struct pbl_temp_stm32h7_adc, struct pbl_sensor, sensor) {     \
    .sensor = PBL_SENSOR_INIT(sym, _name, nullptr, &s_ops, PBL_BIT(PBL_SENSOR_TEMP), 0U, \
                              PBL_SENSOR_MOUNT_IDENTITY),                                \
    .adc = (_adc), .vrefint = _vrefint, .ts_cal1 = _ts_cal1, .ts_cal2 = _ts_cal2,        \
    .state = &sym##_adc_state,                                                           \
  }

#include PBL_DT_INSTANCES(st_stm32h7_adc_sensor)
