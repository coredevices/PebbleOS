/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/charger.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/work.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/sf32lb57_hpsys_aon.h>
#include <hardware/sf32lb57_pmuc.h>

struct pbl_charger_sf32lb57_data {
  struct pbl_work work;
  const struct pbl_charger *charger;
};

struct pbl_charger_sf32lb57 {
  struct pbl_charger charger;
  uintptr_t base;
  pbl_irq_t irq;
  uintptr_t aon;
  struct pbl_charger_sf32lb57_data *data;
};

static const struct pbl_charger_sf32lb57 *prv_chg(const struct pbl_charger *charger) {
  return PBL_CONTAINER_OF(charger, const struct pbl_charger_sf32lb57, charger);
}

static void prv_isr(const struct pbl_charger_sf32lb57 *chg) {
  uint32_t cr5 = pbl_readl(chg->base + SF32LB57_PMUC_CHG_CR5_OFFSET);

  pbl_writel(PBL_FIELD_GET(PMUC_CHG_CR5_IS_MASK, cr5), chg->base + SF32LB57_PMUC_CHG_CR5_OFFSET);
  if ((cr5 & PMUC_CHG_CR5_IS_VBUS_RDY) != 0U) {
    (void)pbl_work_submit(&pbl_sys_workq_high, &chg->data->work);
  }
}

static void prv_online_changed(struct pbl_work *work) {
  struct pbl_charger_sf32lb57_data *data =
      PBL_CONTAINER_OF(work, struct pbl_charger_sf32lb57_data, work);

  pbl_charger_report_online_changed(data->charger);
}

static int prv_init(const struct pbl_charger *charger) {
  const struct pbl_charger_sf32lb57 *chg = prv_chg(charger);

  pbl_writel(PMUC_CHG_CR5_IC_MASK, chg->base + SF32LB57_PMUC_CHG_CR5_OFFSET);
  pbl_modifyl(PMUC_CHG_CR4_IM_VBUS_RDY_MASK,
              PBL_FIELD_PREP(PMUC_CHG_CR4_IM_VBUS_RDY_MASK, PMUC_CHG_IM_DOUBLE_EDGE) |
                  PMUC_CHG_CR4_IE_VBUS_RDY,
              chg->base + SF32LB57_PMUC_CHG_CR4_OFFSET);
  // Deep sleep leaves only AON wake sources: USB power coming must wake HPSYS too
  pbl_modifyl(0U, HPSYS_AON_WER_CHG, chg->aon + SF32LB57_HPSYS_AON_WER_OFFSET);
  pbl_irq_enable(chg->irq);

  return 0;
}

static int prv_is_online(const struct pbl_charger *charger, bool *online) {
  *online = (pbl_readl(prv_chg(charger)->base + SF32LB57_PMUC_CHG_SR_OFFSET) &
             PMUC_CHG_SR_VBUS_RDY_OUT) != 0U;

  return 0;
}

static int prv_get_status(const struct pbl_charger *charger, enum pbl_charger_status *status) {
  uint32_t sr = pbl_readl(prv_chg(charger)->base + SF32LB57_PMUC_CHG_SR_OFFSET);

  if ((sr & PMUC_CHG_SR_EOC_MODE) != 0U) {
    *status = PBL_CHARGER_STATUS_FULL;
  } else if ((sr & PMUC_CHG_SR_CV_MODE) != 0U) {
    *status = PBL_CHARGER_STATUS_CONSTANT_VOLTAGE;
  } else if ((sr & PMUC_CHG_SR_CC_MODE) != 0U) {
    *status = PBL_CHARGER_STATUS_CONSTANT_CURRENT;
  } else {
    *status = PBL_CHARGER_STATUS_UNKNOWN;
  }

  return 0;
}

static const struct pbl_charger_ops s_ops = {
  .init = prv_init,
  .is_online = prv_is_online,
  .get_status = prv_get_status,
};

#define PBL_CHARGER_SF32LB57_DEFINE(sym, _name, _base, _irq, _aon)                    \
  PBL_CHARGER_STATE_DEFINE(sym);                                                      \
  static struct pbl_charger_sf32lb57_data sym##_data;                                 \
  PBL_DEVICE_INSTANCE(sym, struct pbl_charger_sf32lb57, struct pbl_charger, charger){ \
    .charger = PBL_CHARGER_INIT(sym, _name, nullptr, &s_ops),                         \
    .base = (_base),                                                                  \
    .irq = (_irq),                                                                    \
    .aon = (_aon),                                                                    \
    .data = &sym##_data,                                                              \
  };                                                                                  \
  static struct pbl_charger_sf32lb57_data sym##_data = {                              \
    .work = PBL_WORK_INITIALIZER(prv_online_changed),                                 \
    .charger = &sym##_inst.charger,                                                   \
  };                                                                                  \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_CHARGER_SF32LB57_IRQ_PRIO, prv_isr, &sym##_inst, 0)

#include PBL_DT_INSTANCES(sifli_sf32lb57_pmuc_charger)
