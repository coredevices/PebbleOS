/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "canopen_internal.h"

#include <errno.h>
#include <string.h>

#include <pbl/util/byteorder.h>
#include <pbl/util/misc.h>

static uint8_t prv_register(const struct pbl_canopen *co) {
  uint8_t reg = 0U;

  for (uint8_t i = 0U; i < co->num_emcy_active; i++) {
    reg |= co->emcy_active[i].reg | PBL_CANOPEN__ERR_REG_GENERIC;
  }

  return reg;
}

static void prv_transmit(struct pbl_canopen *co, const struct pbl_canopen__emcy *emcy) {
  uint32_t cob = pbl_canopen__od_get_u32(co->cfg.od, PBL_CANOPEN__IDX_EMCY, 0U);
  uint16_t inhibit = (uint16_t)pbl_canopen__od_get_u32(co->cfg.od, PBL_CANOPEN__IDX_EMCY_INH, 0U);
  uint8_t data[8];

  if ((cob & PBL_CANOPEN__COB_INVALID) != 0U) {
    return;
  }

  pbl_put_le16(emcy->code, &data[0]);
  data[2] = emcy->reg;
  memcpy(&data[3], emcy->msef, sizeof(emcy->msef));
  pbl_canopen__send(co, cob, data, sizeof(data));

  if (inhibit != 0U) {
    co->emcy_inhibited = true;
    pbl_delayed_work_reschedule(&pbl_canopen__workq, &co->emcy_inhibit,
                                PBL_TICKS(PBL_US_TO_TICKS_CEIL(inhibit * 100U)));
  }
}

static void prv_send(struct pbl_canopen *co, uint16_t code, uint8_t reg, const uint8_t *msef) {
  struct pbl_canopen__emcy emcy = {.code = code, .reg = reg};

  if (co->nmt != PBL_CANOPEN_NMT_PRE_OPERATIONAL && co->nmt != PBL_CANOPEN_NMT_OPERATIONAL) {
    return;
  }

  if (msef != nullptr) {
    memcpy(emcy.msef, msef, sizeof(emcy.msef));
  }

  if (!co->emcy_inhibited) {
    prv_transmit(co, &emcy);
    return;
  }

  if (co->emcy_count == CONFIG_CANOPEN_EMCY_QUEUE) {
    return;
  }
  co->emcy_queue[(co->emcy_head + co->emcy_count) % CONFIG_CANOPEN_EMCY_QUEUE] = emcy;
  co->emcy_count++;
}

static void prv_inhibit_work(struct pbl_work *work) {
  struct pbl_canopen *co = PBL_CONTAINER_OF(work, struct pbl_canopen, emcy_inhibit.work);

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  co->emcy_inhibited = false;
  if (co->started && co->emcy_count > 0U) {
    struct pbl_canopen__emcy emcy = co->emcy_queue[co->emcy_head];

    co->emcy_head = (uint8_t)((co->emcy_head + 1U) % CONFIG_CANOPEN_EMCY_QUEUE);
    co->emcy_count--;
    prv_transmit(co, &emcy);
  }
  pbl_mutex_unlock(&co->lock);
}

static void prv_history_add(struct pbl_canopen *co, uint16_t code) {
  const struct pbl_canopen_od *od = co->cfg.od;
  uint8_t count;

  if (od->num_errors == 0U) {
    return;
  }

  count = (uint8_t)pbl_canopen__od_get_u32(od, PBL_CANOPEN__IDX_ERRORS, 0U);
  if (count < od->num_errors) {
    count++;
  }
  for (uint8_t i = count; i > 1U; i--) {
    pbl_canopen__od_set_u32(od, PBL_CANOPEN__IDX_ERRORS, i,
                            pbl_canopen__od_get_u32(od, PBL_CANOPEN__IDX_ERRORS, i - 1U));
  }
  pbl_canopen__od_set_u32(od, PBL_CANOPEN__IDX_ERRORS, 1U, code);
  pbl_canopen__od_set_u32(od, PBL_CANOPEN__IDX_ERRORS, 0U, count);
}

void pbl_canopen__emcy_init(struct pbl_canopen *co) {
  pbl_delayed_work_init(&co->emcy_inhibit, prv_inhibit_work);
}

void pbl_canopen__emcy_reset(struct pbl_canopen *co) {
  co->num_emcy_active = 0U;
  co->emcy_count = 0U;
  co->emcy_inhibited = false;
  pbl_delayed_work_cancel(&co->emcy_inhibit);
}

int pbl_canopen__emcy_raise(struct pbl_canopen *co, uint16_t code, uint8_t reg,
                            const uint8_t *msef) {
  uint8_t err_reg;

  for (uint8_t i = 0U; i < co->num_emcy_active; i++) {
    if (co->emcy_active[i].code == code) {
      return -EALREADY;
    }
  }
  if (co->num_emcy_active == CONFIG_CANOPEN_EMCY_ACTIVE) {
    return -ENOSPC;
  }

  co->emcy_active[co->num_emcy_active++] = (struct pbl_canopen__emcy){.code = code, .reg = reg};
  err_reg = prv_register(co);
  pbl_canopen__od_set_u32(co->cfg.od, PBL_CANOPEN__IDX_ERROR_REG, 0U, err_reg);
  prv_history_add(co, code);
  prv_send(co, code, err_reg, msef);

  return 0;
}

int pbl_canopen__emcy_clear(struct pbl_canopen *co, uint16_t code) {
  uint8_t err_reg;
  uint8_t i;

  for (i = 0U; i < co->num_emcy_active; i++) {
    if (co->emcy_active[i].code == code) {
      break;
    }
  }
  if (i == co->num_emcy_active) {
    return -ENOENT;
  }

  co->num_emcy_active--;
  memmove(&co->emcy_active[i], &co->emcy_active[i + 1U],
          (co->num_emcy_active - i) * sizeof(co->emcy_active[0]));
  err_reg = prv_register(co);
  pbl_canopen__od_set_u32(co->cfg.od, PBL_CANOPEN__IDX_ERROR_REG, 0U, err_reg);
  prv_send(co, PBL_CANOPEN__EMCY_RESET, err_reg, nullptr);

  return 0;
}

uint32_t pbl_canopen__emcy_check(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e,
                                 const void *val) {
  uint32_t v = *(const uint32_t *)val;
  uint32_t cur;

  if (e->index == PBL_CANOPEN__IDX_ERRORS) {
    return v != 0U ? PBL_CANOPEN__ABORT_VALUE : 0U;
  }

  cur = pbl_canopen__od_get_u32(co->cfg.od, PBL_CANOPEN__IDX_EMCY, 0U);
  if ((v & PBL_CANOPEN__COB_GEN) != 0U ||
      ((v & PBL_CANOPEN__COB_EXT) == 0U && (v & PBL_CAN_EXT_ID_MASK) > PBL_CAN_STD_ID_MASK)) {
    return PBL_CANOPEN__ABORT_VALUE;
  }
  if ((cur & PBL_CANOPEN__COB_INVALID) == 0U && (v & PBL_CANOPEN__COB_INVALID) == 0U &&
      (cur & PBL_CAN_EXT_ID_MASK) != (v & PBL_CAN_EXT_ID_MASK)) {
    return PBL_CANOPEN__ABORT_VALUE;
  }

  return 0U;
}
