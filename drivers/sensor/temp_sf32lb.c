/* SPDX-FileCopyrightText: 2025 SiFli Technologies(Nanjing) Co., Ltd */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/sensor.h>
#include <pbl/drivers/sensor/temp_sf32lb.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/time.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/sf32lb52_hpsys_cfg.h>
#include <hardware/sf32lb52_tsen.h>

#define CONVERSION_TIMEOUT_US 20000U

// m°C = (DATA + 3000) * 749.2916 / 10100 * 1000 - 277539
#define SLOPE_NUM 2971
#define SLOPE_DEN 40
#define BIAS      (3000 * SLOPE_NUM / SLOPE_DEN - 277539)

static const struct pbl_sensor_chan s_chans[] = {
  PBL_SENSOR_CHAN(PBL_SENSOR_TEMP, 0, 0, PBL_SENSOR_FMT_U16_LE, SLOPE_NUM, SLOPE_DEN, BIAS),
};

static const struct pbl_temp_sf32lb *prv_tsen(const struct pbl_sensor *sensor) {
  return PBL_CONTAINER_OF(sensor, const struct pbl_temp_sf32lb, sensor);
}

static inline uint32_t prv_getreg(const struct pbl_temp_sf32lb *tsen, uint32_t offset) {
  return pbl_readl(tsen->base + offset);
}

static inline void prv_putreg(const struct pbl_temp_sf32lb *tsen, uint32_t offset, uint32_t value) {
  pbl_writel(value, tsen->base + offset);
}

static int prv_init(const struct pbl_sensor *sensor) {
  pbl_modifyl(0U, HPSYS_CFG_ANAU_CR_EN_BG,
              prv_tsen(sensor)->cfg + SF32LB52_HPSYS_CFG_ANAU_CR_OFFSET);

  return 0;
}

static void prv_tsen_enable(const struct pbl_temp_sf32lb *tsen) {
  uint32_t ctrl = prv_getreg(tsen, SF32LB52_TSEN_CTRL_OFFSET);

  ctrl &= ~TSEN_CTRL_RSTB;
  prv_putreg(tsen, SF32LB52_TSEN_CTRL_OFFSET, ctrl);
  ctrl |= TSEN_CTRL_EN | TSEN_CTRL_PU;
  prv_putreg(tsen, SF32LB52_TSEN_CTRL_OFFSET, ctrl);
  ctrl |= TSEN_CTRL_RSTB;
  prv_putreg(tsen, SF32LB52_TSEN_CTRL_OFFSET, ctrl);
  pbl_busy_wait_us(20);
  ctrl |= TSEN_CTRL_RUN;
  prv_putreg(tsen, SF32LB52_TSEN_CTRL_OFFSET, ctrl);
}

static void prv_tsen_disable(const struct pbl_temp_sf32lb *tsen) {
  uint32_t ctrl = prv_getreg(tsen, SF32LB52_TSEN_CTRL_OFFSET);

  prv_putreg(tsen, SF32LB52_TSEN_CTRL_OFFSET, ctrl & ~(TSEN_CTRL_EN | TSEN_CTRL_PU));
}

static int prv_read(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                    struct pbl_sensor_frame *frame) {
  const struct pbl_temp_sf32lb *tsen = prv_tsen(sensor);
  int ret;

  ret = pbl_clk_enable(&tsen->clk);
  if (ret != 0) {
    return ret;
  }

  prv_tsen_enable(tsen);

  ret = PBL_POLL_BUSY_US((prv_getreg(tsen, SF32LB52_TSEN_IRQ_OFFSET) & TSEN_IRQ_IRSR) != 0U, 1,
                         CONVERSION_TIMEOUT_US);
  if (ret == 0) {
    pbl_put_le16((uint16_t)(prv_getreg(tsen, SF32LB52_TSEN_RDATA_OFFSET) & TSEN_RDATA_MASK),
                 frame->data);
    frame->chans = s_chans;
    frame->num_chans = PBL_ARRAY_SIZE(s_chans);
    frame->timestamp_us = pbl_uptime_us();
  }

  prv_putreg(tsen, SF32LB52_TSEN_IRQ_OFFSET,
             prv_getreg(tsen, SF32LB52_TSEN_IRQ_OFFSET) | TSEN_IRQ_ICR);
  prv_tsen_disable(tsen);
  pbl_clk_disable(&tsen->clk);

  return ret;
}

const struct pbl_sensor_ops pbl_temp_sf32lb_ops = {
  .init = prv_init,
  .read = prv_read,
};

#include PBL_DT_INSTANCES(sifli_sf32lb52_tsen)
