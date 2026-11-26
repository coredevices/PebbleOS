/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ipms_canfd_regs.h"

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/can.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/reset.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/sf32lb/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#define NUM_FILTERS 16U

// A prescaler of 1 counts time quanta of two clock periods
#define PRESCALER_MIN 2U

// Error warning limit, (EWL + 1) * 8: 96, as ISO 11898-1 sets it
#define EWL 11U

#define IR_ENABLES                                                                               \
  (CANFD_IR_RIE | CANFD_IR_ROIE | CANFD_IR_RFIE | CANFD_IR_TPIE | CANFD_IR_EIE | CANFD_IR_EPIE | \
   CANFD_IR_BEIE | CANFD_IR_ALIE)

// Variants
#define IPMS_CANFD_SIFLI PBL_BIT(0)

struct pbl_can_ipms_canfd_state {
  struct pbl_can_filter filters[NUM_FILTERS];
  uint32_t bittime;
  uint32_t presc;
  uint32_t cr_mode;
  uint16_t filters_enabled;
  bool manual_recovery;
  enum pbl_can_bus_state bus_state;
  pbl_can_tx_callback_t tx_cb;
  void *tx_arg;
  uint8_t tx_tec;
  bool tx_busy;
  bool tx_failed;
};

struct pbl_can_ipms_canfd {
  struct pbl_can can;
  uintptr_t base;
  pbl_irq_t irq;
  uint32_t quirks;
  struct pbl_clk clk;
  struct pbl_reset rst;
  const struct pbl_pinctrl *pinctrl;
  struct pbl_can_ipms_canfd_state *state;
};

static const struct pbl_can_timing_limits s_limits = {
  .tseg1_min = 3U,
  .tseg1_max = PBL_FIELD_GET(CANFD_BITTIME_S_SEG_1, UINT32_MAX) + 1U,
  .tseg2_min = 2U,
  .tseg2_max = PBL_FIELD_GET(CANFD_BITTIME_S_SEG_2, UINT32_MAX) + 1U,
  .sjw_max = PBL_FIELD_GET(CANFD_BITTIME_S_SJW, UINT32_MAX) + 1U,
  .prescaler_min = PRESCALER_MIN,
  .prescaler_max = PBL_FIELD_GET(CANFD_PRESC_S_PRESC, UINT32_MAX) + 1U,
};

static const struct pbl_can_ipms_canfd *prv_canfd(const struct pbl_can *can) {
  return PBL_CONTAINER_OF(can, const struct pbl_can_ipms_canfd, can);
}

static inline uint32_t prv_read(const struct pbl_can_ipms_canfd *c, uint32_t offset) {
  return pbl_readl(c->base + offset);
}

static inline void prv_write(const struct pbl_can_ipms_canfd *c, uint32_t offset, uint32_t val) {
  pbl_writel(val, c->base + offset);
}

// Write CR without repeating the commands its current value reads back
static void prv_cr_cmd(const struct pbl_can_ipms_canfd *c, uint32_t cmd) {
  prv_write(c, CANFD_CR, (prv_read(c, CANFD_CR) & ~CANFD_CR_CMDS) | cmd);
}

static enum pbl_can_bus_state prv_bus_state(uint32_t cr, uint32_t ir) {
  if ((cr & CANFD_CR_BUSOFF) != 0U) {
    return PBL_CAN_STATE_BUS_OFF;
  }
  if ((ir & CANFD_IR_EPASS) != 0U) {
    return PBL_CAN_STATE_ERROR_PASSIVE;
  }
  if ((ir & CANFD_IR_EWARN) != 0U) {
    return PBL_CAN_STATE_ERROR_WARNING;
  }
  return PBL_CAN_STATE_ERROR_ACTIVE;
}

static struct pbl_can_err_cnt prv_err_cnt(const struct pbl_can_ipms_canfd *c) {
  uint32_t cnt = prv_read(c, CANFD_CNT);

  return (struct pbl_can_err_cnt){
    .tx = (uint8_t)PBL_FIELD_GET(CANFD_CNT_TECNT, cnt),
    .rx = (uint8_t)PBL_FIELD_GET(CANFD_CNT_RECNT, cnt),
  };
}

// The core ignores LOM while in reset, even on the write that leaves it
static void prv_leave_reset(const struct pbl_can_ipms_canfd *c) {
  prv_write(c, CANFD_CR, 0U);
  prv_write(c, CANFD_CR, c->state->cr_mode);
}

static void prv_tx_complete(const struct pbl_can_ipms_canfd *c, int status) {
  struct pbl_can_ipms_canfd_state *state = c->state;

  if (!state->tx_busy) {
    return;
  }

  state->tx_busy = false;
  pbl_can_tx_done(&c->can, state->tx_cb, state->tx_arg, status);
}

static void prv_rx(const struct pbl_can_ipms_canfd *c) {
  struct pbl_can_frame frame;
  uint32_t id, ctrl, dlc;

  while ((prv_read(c, CANFD_CR) & CANFD_CR_RSTAT) != 0U) {
    id = prv_read(c, CANFD_RBUF);
    ctrl = prv_read(c, CANFD_RBUF + 4U);
    dlc = PBL_FIELD_GET(CANFD_BUF_CTRL_DLC, ctrl);

    frame.flags = 0U;
    if ((ctrl & CANFD_BUF_CTRL_IDE) != 0U) {
      frame.flags |= PBL_CAN_FRAME_IDE;
      frame.id = id & PBL_CAN_EXT_ID_MASK;
    } else {
      frame.id = id & PBL_CAN_STD_ID_MASK;
    }

    if ((ctrl & CANFD_BUF_CTRL_RTR) != 0U) {
      frame.flags |= PBL_CAN_FRAME_RTR;
    }
    frame.len = (uint8_t)PBL_MIN(dlc, PBL_CAN_MAX_LEN);

    if ((frame.flags & PBL_CAN_FRAME_RTR) == 0U) {
      for (uint32_t i = 0U; i < PBL_DIV_ROUND_UP(frame.len, 4U); i++) {
        frame.data32[i] = prv_read(c, CANFD_RBUF + (CANFD_BUF_DATA + i) * 4U);
      }
    }

    prv_cr_cmd(c, CANFD_CR_RREL);

    pbl_can_receive(&c->can, &frame);
  }
}

static void prv_update_state(const struct pbl_can_ipms_canfd *c, uint32_t cr, uint32_t ir) {
  struct pbl_can_ipms_canfd_state *state = c->state;
  enum pbl_can_bus_state bus_state = prv_bus_state(cr, ir);

  // Reset clears the bus off flag and the error counters: still bus off until leaving it
  if (state->bus_state == PBL_CAN_STATE_BUS_OFF && (cr & CANFD_CR_RESET) != 0U) {
    return;
  }

  if (bus_state == state->bus_state) {
    return;
  }

  state->bus_state = bus_state;

  // Bus off puts the core in reset, dropping the frame; leaving it starts the recovery
  if (bus_state == PBL_CAN_STATE_BUS_OFF) {
    prv_tx_complete(c, -ENETDOWN);
    if (!state->manual_recovery) {
      prv_leave_reset(c);
    }
  }

  pbl_can_report_state(&c->can, bus_state,
                       (bus_state == PBL_CAN_STATE_BUS_OFF)
                           ? (struct pbl_can_err_cnt){.tx = UINT8_MAX}
                           : prv_err_cnt(c));
}

// A single shot frame completes even if it failed: an error only a transmitter gets, or a rise
// of the transmit error counter, marks it failed
static void prv_bus_error(const struct pbl_can_ipms_canfd *c) {
  struct pbl_can_ipms_canfd_state *state = c->state;
  uint32_t cnt = prv_read(c, CANFD_CNT);
  uint32_t koer = PBL_FIELD_GET(CANFD_CNT_KOER, cnt);

  if (state->tx_busy && (koer == CANFD_KOER_BIT || koer == CANFD_KOER_ACK ||
                         PBL_FIELD_GET(CANFD_CNT_TECNT, cnt) > state->tx_tec)) {
    state->tx_failed = true;
  }

  switch (koer) {
    case CANFD_KOER_BIT:
      pbl_can_report_error(&c->can, PBL_CAN_ERROR_BIT);
      break;
    case CANFD_KOER_FORM:
      pbl_can_report_error(&c->can, PBL_CAN_ERROR_FORM);
      break;
    case CANFD_KOER_STUFF:
      pbl_can_report_error(&c->can, PBL_CAN_ERROR_STUFF);
      break;
    case CANFD_KOER_ACK:
      pbl_can_report_error(&c->can, PBL_CAN_ERROR_ACK);
      break;
    case CANFD_KOER_CRC:
      pbl_can_report_error(&c->can, PBL_CAN_ERROR_CRC);
      break;
    default:
      break;
  }
}

static void prv_isr(const struct pbl_can_ipms_canfd *c) {
  uint32_t ir = prv_read(c, CANFD_IR);
  uint32_t flags = ir & CANFD_IR_FLAGS;

  prv_write(c, CANFD_IR, (ir & ~CANFD_IR_FLAGS) | flags);

  if ((flags & CANFD_IR_ROIF) != 0U) {
    pbl_can_report_error(&c->can, PBL_CAN_ERROR_RX_OVERRUN);
  }

  if ((flags & (CANFD_IR_RIF | CANFD_IR_RFIF | CANFD_IR_ROIF)) != 0U) {
    prv_rx(c);
  }

  if ((flags & CANFD_IR_BEIF) != 0U) {
    prv_bus_error(c);
  }
  if ((flags & CANFD_IR_ALIF) != 0U) {
    if (c->state->tx_busy) {
      c->state->tx_failed = true;
    }
    pbl_can_report_error(&c->can, PBL_CAN_ERROR_ARBITRATION_LOST);
  }

  if ((flags & CANFD_IR_TPIF) != 0U) {
    prv_tx_complete(c,
                    ((c->state->cr_mode & CANFD_CR_TPSS) != 0U && c->state->tx_failed) ? -EIO : 0);
  } else if ((flags & CANFD_IR_AIF) != 0U) {
    prv_tx_complete(c, -ECANCELED);
  }

  if ((flags & (CANFD_IR_EIF | CANFD_IR_EPIF)) != 0U) {
    prv_update_state(c, prv_read(c, CANFD_CR), ir);
  }
}

static int prv_init(const struct pbl_can *can) {
  const struct pbl_can_ipms_canfd *c = prv_canfd(can);
  int ret;

  ret = pbl_device_init(&c->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&c->rst.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&c->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_reset_reset(&c->rst);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(c->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  prv_write(c, CANFD_CR, CANFD_CR_RESET);
  prv_write(c, CANFD_IR, CANFD_IR_FLAGS);

  pbl_irq_enable(c->irq);

  return 0;
}

static int prv_get_core_clock(const struct pbl_can *can, uint32_t *rate) {
  const struct pbl_can_ipms_canfd *c = prv_canfd(can);
  uint64_t rate_mhz;
  int ret;

  ret = pbl_clk_get_rate(&c->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  *rate = (uint32_t)(rate_mhz / 1000U);

  return 0;
}

static int prv_configure(const struct pbl_can *can, const struct pbl_can_timing *nominal,
                         const struct pbl_can_timing *data, uint32_t mode) {
  struct pbl_can_ipms_canfd_state *state = prv_canfd(can)->state;
  uint32_t bittime, presc;

  bittime = PBL_FIELD_PREP(CANFD_BITTIME_S_SEG_1, nominal->tseg1 - 1U) |
            PBL_FIELD_PREP(CANFD_BITTIME_S_SEG_2, nominal->tseg2 - 1U) |
            PBL_FIELD_PREP(CANFD_BITTIME_S_SJW, nominal->sjw - 1U);
  presc = PBL_FIELD_PREP(CANFD_PRESC_S_PRESC, nominal->prescaler - 1U);

  state->bittime = bittime;
  state->presc = presc;
  state->cr_mode = 0U;
  if ((mode & PBL_CAN_MODE_LISTEN_ONLY) != 0U) {
    state->cr_mode |= CANFD_CR_LOM;
  }
  if ((mode & PBL_CAN_MODE_ONE_SHOT) != 0U) {
    state->cr_mode |= CANFD_CR_TPSS;
  }
  state->manual_recovery = (mode & PBL_CAN_MODE_MANUAL_RECOVERY) != 0U;

  return 0;
}

static int prv_set_filter(const struct pbl_can *can, uint8_t idx,
                          const struct pbl_can_filter *filter) {
  struct pbl_can_ipms_canfd_state *state = prv_canfd(can)->state;

  if (filter != nullptr) {
    state->filters[idx] = *filter;
    state->filters_enabled |= PBL_BIT(idx);
  } else {
    state->filters_enabled &= ~PBL_BIT(idx);
  }

  return 0;
}

// The core matches the bits its mask has clear, and checks the IDE bit with AIDEE
static void prv_write_filters(const struct pbl_can_ipms_canfd *c) {
  struct pbl_can_ipms_canfd_state *state = c->state;
  uint32_t enabled = 0U;

  if (state->filters_enabled == 0U) {
    prv_write(c, CANFD_ACFCTRL, CANFD_ACFCTRL_SELMASK | 0U);
    prv_write(c, CANFD_ACF, CANFD_ACF_ACODE);
    prv_write(c, CANFD_ACFCTRL, 0U);
    prv_write(c, CANFD_ACF, 0U);
    prv_write(c, CANFD_ACFCTRL, CANFD_ACFCTRL_AE(0U));
    return;
  }

  for (uint32_t i = 0U; i < NUM_FILTERS; i++) {
    const struct pbl_can_filter *f = &state->filters[i];
    uint32_t mask, aide;

    if ((state->filters_enabled & PBL_BIT(i)) == 0U) {
      continue;
    }

    if ((f->flags & PBL_CAN_FRAME_IDE) != 0U) {
      mask = ~f->mask & PBL_CAN_EXT_ID_MASK;
      aide = CANFD_ACF_AIDE;
    } else {
      mask = (~f->mask & PBL_CAN_STD_ID_MASK) | (CANFD_ACF_ACODE & ~PBL_CAN_STD_ID_MASK);
      aide = 0U;
    }

    prv_write(c, CANFD_ACFCTRL, CANFD_ACFCTRL_SELMASK | PBL_FIELD_PREP(CANFD_ACFCTRL_ACFADR, i));
    prv_write(c, CANFD_ACF, mask | CANFD_ACF_AIDEE | aide);
    prv_write(c, CANFD_ACFCTRL, PBL_FIELD_PREP(CANFD_ACFCTRL_ACFADR, i));
    prv_write(c, CANFD_ACF, f->id & f->mask);
    enabled |= CANFD_ACFCTRL_AE(i);
  }

  prv_write(c, CANFD_ACFCTRL, enabled);
}

static int prv_start(const struct pbl_can *can) {
  const struct pbl_can_ipms_canfd *c = prv_canfd(can);
  struct pbl_can_ipms_canfd_state *state = c->state;

  // Only a block reset clears the error counters
  (void)pbl_reset_reset(&c->rst);
  prv_write(c, CANFD_CR, CANFD_CR_RESET);

  // MEMMASK in CR2 drops the received frames
  if ((c->quirks & IPMS_CANFD_SIFLI) != 0U) {
    prv_write(c, CANFD_CR2, 0U);
  }

  prv_write(c, CANFD_BITTIME, state->bittime);
  prv_write(c, CANFD_PRESC, state->presc);
  prv_write_filters(c);

  if ((c->quirks & IPMS_CANFD_SIFLI) != 0U) {
    pbl_idle_state_block(SOC_SF32LB_IDLE_DEEPSLEEP);
  }

  state->bus_state = PBL_CAN_STATE_ERROR_ACTIVE;
  state->tx_busy = false;

  prv_leave_reset(c);
  prv_write(c, CANFD_IR, CANFD_IR_FLAGS | IR_ENABLES | PBL_FIELD_PREP(CANFD_IR_EWL, EWL));

  return 0;
}

static void prv_stop(const struct pbl_can *can) {
  const struct pbl_can_ipms_canfd *c = prv_canfd(can);

  prv_write(c, CANFD_IR, CANFD_IR_FLAGS);
  prv_write(c, CANFD_CR, CANFD_CR_RESET);

  if ((c->quirks & IPMS_CANFD_SIFLI) != 0U) {
    pbl_idle_state_release(SOC_SF32LB_IDLE_DEEPSLEEP);
  }

  prv_tx_complete(c, -ENETDOWN);
}

static int prv_send(const struct pbl_can *can, const struct pbl_can_frame *frame,
                    pbl_can_tx_callback_t cb, void *arg) {
  const struct pbl_can_ipms_canfd *c = prv_canfd(can);
  struct pbl_can_ipms_canfd_state *state = c->state;
  uint32_t cr = prv_read(c, CANFD_CR);
  uint32_t ctrl, words = 0U;

  if ((cr & CANFD_CR_RESET) != 0U) {
    return -ENETDOWN;
  }

  if (state->tx_busy || (cr & CANFD_CR_TPE) != 0U) {
    return -EBUSY;
  }

  ctrl = PBL_FIELD_PREP(CANFD_BUF_CTRL_DLC, pbl_can_len_to_dlc(frame->len));
  if ((frame->flags & PBL_CAN_FRAME_IDE) != 0U) {
    ctrl |= CANFD_BUF_CTRL_IDE;
  }
  if ((frame->flags & PBL_CAN_FRAME_RTR) != 0U) {
    ctrl |= CANFD_BUF_CTRL_RTR;
  } else {
    words = PBL_DIV_ROUND_UP(frame->len, 4U);
  }

  prv_write(c, CANFD_TBUF, frame->id);
  prv_write(c, CANFD_TBUF + 4U, ctrl);
  for (uint32_t i = 0U; i < words; i++) {
    prv_write(c, CANFD_TBUF + (CANFD_BUF_DATA + i) * 4U, frame->data32[i]);
  }

  state->tx_cb = cb;
  state->tx_arg = arg;
  state->tx_tec = (uint8_t)PBL_FIELD_GET(CANFD_CNT_TECNT, prv_read(c, CANFD_CNT));
  state->tx_failed = false;
  state->tx_busy = true;

  prv_cr_cmd(c, CANFD_CR_TPE);

  return 0;
}

static void prv_abort(const struct pbl_can *can) {
  const struct pbl_can_ipms_canfd *c = prv_canfd(can);

  // A frame that keeps failing ignores TPA: reset drops it
  if (c->state->tx_busy) {
    prv_write(c, CANFD_CR, CANFD_CR_RESET);
    prv_leave_reset(c);
    prv_tx_complete(c, -ECANCELED);
  }
}

static int prv_recover(const struct pbl_can *can) {
  prv_leave_reset(prv_canfd(can));

  return 0;
}

static int prv_get_state(const struct pbl_can *can, enum pbl_can_bus_state *state,
                         struct pbl_can_err_cnt *cnt) {
  const struct pbl_can_ipms_canfd *c = prv_canfd(can);

  if (c->state->bus_state == PBL_CAN_STATE_BUS_OFF &&
      (prv_read(c, CANFD_CR) & CANFD_CR_RESET) != 0U) {
    *state = PBL_CAN_STATE_BUS_OFF;
    *cnt = (struct pbl_can_err_cnt){.tx = UINT8_MAX};
    return 0;
  }

  *state = prv_bus_state(prv_read(c, CANFD_CR), prv_read(c, CANFD_IR));
  *cnt = prv_err_cnt(c);

  return 0;
}

static const struct pbl_can_ops s_ops = {
  .init = prv_init,
  .get_core_clock = prv_get_core_clock,
  .configure = prv_configure,
  .set_filter = prv_set_filter,
  .start = prv_start,
  .stop = prv_stop,
  .send = prv_send,
  .abort = prv_abort,
  .get_state = prv_get_state,
  .recover = prv_recover,
};

// Classic CAN only: the SF32LB57 core supports neither loopback nor CAN FD
#define MODES (PBL_CAN_MODE_LISTEN_ONLY | PBL_CAN_MODE_ONE_SHOT | PBL_CAN_MODE_MANUAL_RECOVERY)

#define PBL_CAN_IPMS_CANFD_DEFINE(sym, _name, _base, _irq, _quirks, _clk, _rst, _pinctrl)     \
  PBL_CAN_STATE_DEFINE(sym, 1U);                                                              \
  static struct pbl_can_ipms_canfd_state sym##_canfd_state;                                   \
  static const struct pbl_pinctrl sym##_canfd_pins = PBL_PINCTRL(_pinctrl, nullptr, 0);       \
  PBL_DEVICE_INSTANCE(sym, struct pbl_can_ipms_canfd, struct pbl_can, can){                   \
    .can = PBL_CAN_INIT(sym, _name, nullptr, &s_ops, &s_limits, nullptr, MODES, NUM_FILTERS), \
    .base = (_base),                                                                          \
    .irq = (_irq),                                                                            \
    .quirks = (_quirks),                                                                      \
    .clk = _clk,                                                                              \
    .rst = _rst,                                                                              \
    .pinctrl = &sym##_canfd_pins,                                                             \
    .state = &sym##_canfd_state,                                                              \
  };                                                                                          \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_CAN_IPMS_CANFD_IRQ_PRIO, prv_isr, &sym##_inst, 0)

#include PBL_DT_INSTANCES(sifli_sf32lb57_can)
