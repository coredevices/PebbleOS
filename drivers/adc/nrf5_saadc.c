/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/adc/nrf5_saadc.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <dt-bindings/iio/adc/nordic,nrf52832-saadc.h>
#include <hardware/nrf52_saadc.h>

// Conversions run on the first channel, set up for the input each read asks for
#define CH 0U

// Gain 1/6 against the 0.6 V internal reference: 3.6 V full scale, in 12 bits
#define FULL_SCALE_UV 3600000LL
#define RESULT_BITS   12U
#define VDDH_DIVIDER  5

#define EVENT_TIMEOUT_US     100U
#define CALIBRATE_TIMEOUT_US 10000U

static const struct pbl_adc_nrf5_saadc *prv_saadc(const struct pbl_adc *adc) {
  return PBL_CONTAINER_OF(adc, const struct pbl_adc_nrf5_saadc, adc);
}

static inline uint32_t prv_getreg(const struct pbl_adc_nrf5_saadc *saadc, uint32_t offset) {
  return pbl_readl(saadc->base + offset);
}

static inline void prv_putreg(const struct pbl_adc_nrf5_saadc *saadc, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, saadc->base + offset);
}

// Trigger a task and wait for the event that ends it
static int prv_task(const struct pbl_adc_nrf5_saadc *saadc, uint32_t task, uint32_t event,
                    uint32_t timeout_us) {
  int ret;

  prv_putreg(saadc, event, 0U);
  prv_putreg(saadc, task, 1U);
  ret = PBL_POLL_BUSY_US(prv_getreg(saadc, event) != 0U, 1U, timeout_us);
  prv_putreg(saadc, event, 0U);

  return ret;
}

static uint32_t prv_psel(uint32_t channel) {
  switch (channel) {
    case NRF52_SAADC_VDD:
      return SAADC_PSEL_VDD;
    case NRF52_SAADC_VDDHDIV5:
      return SAADC_PSEL_VDDHDIV5;
    default:
      return SAADC_PSEL_AIN(channel);
  }
}

static int prv_read(const struct pbl_adc *adc, uint32_t channel, int32_t *microvolt) {
  const struct pbl_adc_nrf5_saadc *saadc = prv_saadc(adc);
  int ret;

  prv_putreg(saadc, NRF52_SAADC_CH_PSELP_OFFSET(CH), prv_psel(channel));
  prv_putreg(saadc, NRF52_SAADC_RESULT_PTR_OFFSET, (uint32_t)(uintptr_t)saadc->result);
  prv_putreg(saadc, NRF52_SAADC_RESULT_MAXCNT_OFFSET, 1U);
  prv_putreg(saadc, NRF52_SAADC_ENABLE_OFFSET, SAADC_ENABLE_ENABLE);

  ret = prv_task(saadc, NRF52_SAADC_TASKS_START_OFFSET, NRF52_SAADC_EVENTS_STARTED_OFFSET,
                 EVENT_TIMEOUT_US);
  if (ret == 0) {
    ret = prv_task(saadc, NRF52_SAADC_TASKS_SAMPLE_OFFSET, NRF52_SAADC_EVENTS_END_OFFSET,
                   EVENT_TIMEOUT_US);
  }

  (void)prv_task(saadc, NRF52_SAADC_TASKS_STOP_OFFSET, NRF52_SAADC_EVENTS_STOPPED_OFFSET,
                 EVENT_TIMEOUT_US);
  prv_putreg(saadc, NRF52_SAADC_ENABLE_OFFSET, 0U);
  prv_putreg(saadc, NRF52_SAADC_CH_PSELP_OFFSET(CH), SAADC_PSEL_NC);

  if (ret != 0) {
    return ret;
  }

  *microvolt = (int32_t)(((int64_t)*saadc->result * FULL_SCALE_UV) >> RESULT_BITS);
  if (channel == NRF52_SAADC_VDDHDIV5) {
    *microvolt *= VDDH_DIVIDER;
  }

  return 0;
}

static int prv_init(const struct pbl_adc *adc) {
  const struct pbl_adc_nrf5_saadc *saadc = prv_saadc(adc);
  int ret;

  prv_putreg(saadc, NRF52_SAADC_INTENCLR_OFFSET, 0xFFFFFFFFU);

  for (uint32_t ch = 0U; ch < NRF52_SAADC_NUM_CH; ch++) {
    prv_putreg(saadc, NRF52_SAADC_CH_PSELP_OFFSET(ch), SAADC_PSEL_NC);
    prv_putreg(saadc, NRF52_SAADC_CH_PSELN_OFFSET(ch), SAADC_PSEL_NC);
  }

  // Single-ended, without resistor ladders
  prv_putreg(saadc, NRF52_SAADC_CH_CONFIG_OFFSET(CH),
             PBL_FIELD_PREP(SAADC_CH_CONFIG_GAIN_MASK, SAADC_CH_CONFIG_GAIN_1_6) |
                 PBL_FIELD_PREP(SAADC_CH_CONFIG_TACQ_MASK, SAADC_CH_CONFIG_TACQ_10US));
  prv_putreg(saadc, NRF52_SAADC_RESOLUTION_OFFSET, SAADC_RESOLUTION_12BIT);
  prv_putreg(saadc, NRF52_SAADC_OVERSAMPLE_OFFSET, 0U);
  prv_putreg(saadc, NRF52_SAADC_SAMPLERATE_OFFSET, 0U);

  prv_putreg(saadc, NRF52_SAADC_ENABLE_OFFSET, SAADC_ENABLE_ENABLE);
  ret = prv_task(saadc, NRF52_SAADC_TASKS_CALIBRATEOFFSET_OFFSET,
                 NRF52_SAADC_EVENTS_CALIBRATEDONE_OFFSET, CALIBRATE_TIMEOUT_US);
  prv_putreg(saadc, NRF52_SAADC_ENABLE_OFFSET, 0U);

  return ret;
}

const struct pbl_adc_ops pbl_adc_nrf5_saadc_ops = {
  .init = prv_init,
  .read = prv_read,
};

#include PBL_DT_INSTANCES(nordic_nrf52832_saadc)
