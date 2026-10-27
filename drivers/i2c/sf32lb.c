/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/i2c/sf32lb.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/mcu/cache.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/sf32lb52_i2c.h>

// The controller clock is always 48 MHz on the SF32LB52
#define I2C_CLK_HZ 48000000U

#define RESET_TIMEOUT_US 1000U

#define IER_PIO (I2C_IER_TEIE | I2C_IER_RFIE | I2C_IER_BEDIE | I2C_IER_MSDIE)
#define IER_DMA (I2C_IER_DMADONEIE | I2C_IER_BEDIE | I2C_IER_MSDIE)

static const struct {
  uintptr_t base;
  uint32_t clk;
  pbl_irq_t irqn;
} s_instances[] = {
  {SF32LB52_I2C1_BASE, SF32LB52_HPSYS_CLK_I2C1, PBL_IRQN(I2C1)},
  {SF32LB52_I2C2_BASE, SF32LB52_HPSYS_CLK_I2C2, PBL_IRQN(I2C2)},
  {SF32LB52_I2C3_BASE, SF32LB52_HPSYS_CLK_I2C3, PBL_IRQN(I2C3)},
  {SF32LB52_I2C4_BASE, SF32LB52_HPSYS_CLK_I2C4, PBL_IRQN(I2C4)},
};

static const struct pbl_i2c_sf32lb *prv_i2c(const struct pbl_i2c_bus *bus) {
  return container_of(bus, const struct pbl_i2c_sf32lb, bus);
}

static inline uintptr_t prv_base(const struct pbl_i2c_sf32lb *i2c) {
  return s_instances[i2c->index - 1U].base;
}

static inline uint32_t prv_getreg(const struct pbl_i2c_sf32lb *i2c, uint32_t offset) {
  return pbl_readl(prv_base(i2c) + offset);
}

static inline void prv_putreg(const struct pbl_i2c_sf32lb *i2c, uint32_t offset, uint32_t value) {
  pbl_writel(value, prv_base(i2c) + offset);
}

static struct pbl_clk prv_clk(const struct pbl_i2c_sf32lb *i2c) {
  return (struct pbl_clk)PBL_CLK(SF32LB_HPSYS_RCC, s_instances[i2c->index - 1U].clk);
}

static void prv_sleep_allow(const struct pbl_i2c_sf32lb *i2c) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;
  bool blocked;

  pbl_irq_lock();
  blocked = state->sleep_blocked;
  state->sleep_blocked = false;
  pbl_irq_unlock();

  if (blocked) {
    soc_sf32lb_sleep_release(SOC_SF32LB_DEEPSLEEP);
  }
}

static void prv_configure(const struct pbl_i2c_sf32lb *i2c) {
  uint32_t flv = ((I2C_CLK_HZ + i2c->clock_hz / 2U) / i2c->clock_hz - 7U + 1U) / 2U;
  uint32_t cnt = flv / 2U;

  cnt = (cnt < 3U) ? 0U : cnt - 3U;

  prv_putreg(i2c, SF32LB52_I2C_IER_OFFSET, 0U);
  prv_putreg(i2c, SF32LB52_I2C_LCR_OFFSET,
             PBL_FIELD_PREP(I2C_LCR_FLV_MASK, flv) | I2C_LCR_SLV_MASK);
  pbl_modifyl(I2C_WCR_CNT_MASK, PBL_FIELD_PREP(I2C_WCR_CNT_MASK, cnt),
              prv_base(i2c) + SF32LB52_I2C_WCR_OFFSET);
  prv_putreg(i2c, SF32LB52_I2C_SAR_OFFSET, 0U);
  prv_putreg(
      i2c, SF32LB52_I2C_CR_OFFSET,
      PBL_FIELD_PREP(I2C_CR_MODE_MASK, I2C_CR_MODE_FS) | I2C_CR_SCLE | I2C_CR_MSDE | I2C_CR_IUE);
}

// Unit reset, then a bus reset (nine clocks and a STOP) to free a stuck target
static void prv_reset(const struct pbl_i2c_sf32lb *i2c) {
  uintptr_t cr = prv_base(i2c) + SF32LB52_I2C_CR_OFFSET;

  prv_putreg(i2c, SF32LB52_I2C_IER_OFFSET, 0U);

  pbl_modifyl(0U, I2C_CR_UR, cr);
  pbl_busy_wait_us(1U);
  pbl_modifyl(I2C_CR_UR, 0U, cr);

  pbl_modifyl(0U, I2C_CR_RSTREQ, cr);
  (void)PBL_POLL_BUSY_US((pbl_readl(cr) & I2C_CR_RSTREQ) == 0U, 1U, RESET_TIMEOUT_US);

  pbl_modifyl(0U, I2C_CR_UR, cr);
  pbl_busy_wait_us(1U);
  pbl_modifyl(I2C_CR_UR, 0U, cr);

  prv_putreg(i2c, SF32LB52_I2C_SR_OFFSET, prv_getreg(i2c, SF32LB52_I2C_SR_OFFSET));
  prv_configure(i2c);
}

static bool prv_is_last(const struct pbl_i2c_sf32lb_state *state, uint8_t idx) {
  return idx + 1U == state->num_msgs;
}

static bool prv_continues(const struct pbl_i2c_sf32lb_state *state, uint8_t idx) {
  return idx + 1U < state->num_msgs && (state->msgs[idx + 1U].flags & PBL_I2C_MSG_NOSTART) != 0U;
}

static void prv_send_addr(const struct pbl_i2c_sf32lb *i2c) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;
  const struct pbl_i2c_msg *msg = &state->msgs[state->idx];
  bool read = (msg->flags & PBL_I2C_MSG_READ) != 0U;
  uint32_t tcr = I2C_TCR_START | I2C_TCR_TB;

  state->addr_phase = true;
  state->pos = 0U;
  prv_putreg(i2c, SF32LB52_I2C_DBR_OFFSET, (uint32_t)(state->addr << 1) | (read ? 1U : 0U));
  prv_putreg(i2c, SF32LB52_I2C_TCR_OFFSET, tcr);
}

// Move on to the message after the current one. After the last, the STOP sent with the last
// byte ends the transfer, or a master abort sends it when there was no data.
static void prv_next_msg(const struct pbl_i2c_sf32lb *i2c) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;

  state->idx++;
  if (state->idx < state->num_msgs) {
    prv_send_addr(i2c);
  } else if (!state->stop_sent) {
    prv_putreg(i2c, SF32LB52_I2C_TCR_OFFSET, I2C_TCR_MA);
  }
}

// Send the next byte of the write made of the current message and the ones continuing it.
// Returns false once it is all sent.
static bool prv_write_next(const struct pbl_i2c_sf32lb *i2c) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;
  uint32_t tcr = I2C_TCR_TB;
  uint8_t byte;
  uint8_t idx;
  uint32_t pos;

  while (state->pos >= state->msgs[state->idx].len) {
    if (!prv_continues(state, state->idx)) {
      return false;
    }
    state->idx++;
    state->pos = 0U;
  }

  byte = state->msgs[state->idx].buf[state->pos++];

  idx = state->idx;
  pos = state->pos;
  while (pos >= state->msgs[idx].len && prv_continues(state, idx)) {
    idx++;
    pos = 0U;
  }
  if (pos >= state->msgs[idx].len && prv_is_last(state, idx)) {
    tcr |= I2C_TCR_STOP;
    state->stop_sent = true;
  }

  prv_putreg(i2c, SF32LB52_I2C_DBR_OFFSET, byte);
  prv_putreg(i2c, SF32LB52_I2C_TCR_OFFSET, tcr);

  return true;
}

// Request the next byte of the current read, NACKing the last one
static void prv_read_request(const struct pbl_i2c_sf32lb *i2c) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;
  uint32_t tcr = I2C_TCR_TB;

  if (state->pos + 1U == state->msgs[state->idx].len) {
    tcr |= I2C_TCR_NACK;
    if (prv_is_last(state, state->idx)) {
      tcr |= I2C_TCR_STOP;
      state->stop_sent = true;
    }
  }

  prv_putreg(i2c, SF32LB52_I2C_TCR_OFFSET, tcr);
}

static void prv_dma_chunk(const struct pbl_i2c_sf32lb *i2c) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;
  uint32_t left = state->msgs[state->idx].len - state->pos;
  uint32_t cr = I2C_CR_DMAEN;

  state->chunk = MIN(left, I2C_DNR_NDT_MASK);
  if (state->chunk == left) {
    cr |= I2C_CR_LASTNACK;
    if (prv_is_last(state, state->idx)) {
      cr |= I2C_CR_LASTSTOP;
      state->stop_sent = true;
    }
  }

  prv_putreg(i2c, SF32LB52_I2C_SR_OFFSET, I2C_SR_DMADONE);
  pbl_modifyl(I2C_CR_DMAEN | I2C_CR_LASTNACK | I2C_CR_LASTSTOP, cr,
              prv_base(i2c) + SF32LB52_I2C_CR_OFFSET);
  prv_putreg(i2c, SF32LB52_I2C_DNR_OFFSET, state->chunk);
  prv_putreg(i2c, SF32LB52_I2C_TCR_OFFSET, I2C_TCR_RXREQ);
}

static int prv_dma_start(const struct pbl_i2c_sf32lb *i2c) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;
  const struct pbl_i2c_msg *msg = &state->msgs[state->idx];
  int ret;

  // Dropping the buffer's lines now keeps a dirty one from being evicted over the DMA data
  dcache_invalidate(msg->buf, DCACHE_ROUND_UP(msg->len));

  ret =
      pbl_dma_prep_slave_single(i2c->dma, msg->buf, msg->len, PBL_DMA_DEV_TO_MEM, nullptr, nullptr);
  if (ret != 0) {
    return ret;
  }

  state->dma = true;
  prv_putreg(i2c, SF32LB52_I2C_IER_OFFSET, IER_DMA);
  prv_dma_chunk(i2c);

  return 0;
}

static void prv_dma_finish(const struct pbl_i2c_sf32lb *i2c) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;
  const struct pbl_i2c_msg *msg = &state->msgs[state->idx];

  (void)pbl_dma_terminate(i2c->dma);
  pbl_modifyl(I2C_CR_DMAEN | I2C_CR_LASTNACK | I2C_CR_LASTSTOP, 0U,
              prv_base(i2c) + SF32LB52_I2C_CR_OFFSET);
  dcache_invalidate(msg->buf, DCACHE_ROUND_UP(msg->len));
  state->dma = false;
}

static bool prv_use_dma(const struct pbl_i2c_sf32lb *i2c, const struct pbl_i2c_msg *msg) {
  return i2c->dma != nullptr && (msg->flags & PBL_I2C_MSG_DMA_SAFE) != 0U &&
         msg->len >= I2C_SF32LB_DMA_MIN_BYTES;
}

// An address or data byte went out, or a data byte came in
static void prv_byte_done(const struct pbl_i2c_sf32lb *i2c) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;
  struct pbl_i2c_msg *msg = &state->msgs[state->idx];
  bool read = (msg->flags & PBL_I2C_MSG_READ) != 0U;

  if (state->addr_phase) {
    state->addr_phase = false;

    if (!read) {
      if (!prv_write_next(i2c)) {
        prv_next_msg(i2c);
      }
    } else if (msg->len == 0U) {
      prv_next_msg(i2c);
    } else if (!prv_use_dma(i2c, msg) || prv_dma_start(i2c) != 0) {
      prv_read_request(i2c);
    }
    return;
  }

  if (read) {
    msg->buf[state->pos++] = (uint8_t)prv_getreg(i2c, SF32LB52_I2C_DBR_OFFSET);
    if (state->pos < msg->len) {
      prv_read_request(i2c);
    } else {
      prv_next_msg(i2c);
    }
  } else if (!prv_write_next(i2c)) {
    prv_next_msg(i2c);
  }
}

static void prv_dma_done(const struct pbl_i2c_sf32lb *i2c) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;

  state->pos += state->chunk;
  if (state->pos < state->msgs[state->idx].len) {
    prv_dma_chunk(i2c);
    return;
  }

  prv_dma_finish(i2c);
  prv_putreg(i2c, SF32LB52_I2C_IER_OFFSET, IER_PIO);
  prv_next_msg(i2c);
}

static void prv_complete(const struct pbl_i2c_sf32lb *i2c, int status) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;

  prv_putreg(i2c, SF32LB52_I2C_IER_OFFSET, 0U);
  if (state->dma) {
    prv_dma_finish(i2c);
  }
  prv_sleep_allow(i2c);

  pbl_i2c_bus_complete(&i2c->bus, status);
}

void pbl_i2c_sf32lb_irq_handler(const struct pbl_i2c_sf32lb *i2c) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;
  uint32_t sr = prv_getreg(i2c, SF32LB52_I2C_SR_OFFSET);

  prv_putreg(i2c, SF32LB52_I2C_SR_OFFSET, sr);

  if (prv_getreg(i2c, SF32LB52_I2C_IER_OFFSET) == 0U) {
    return;
  }

  // On a NACK the controller sends the STOP itself, which ends the transfer below
  if ((sr & I2C_SR_BED) != 0U && state->error == 0) {
    state->error = state->addr_phase ? -ENXIO : -EIO;
    prv_putreg(i2c, SF32LB52_I2C_IER_OFFSET, I2C_IER_MSDIE);
  }

  if (state->error == 0) {
    if (state->dma) {
      // The STOP after the last chunk may be seen before its DMA done flag
      if ((sr & (I2C_SR_DMADONE | I2C_SR_MSD)) != 0U) {
        prv_dma_done(i2c);
      }
    } else if ((sr & (I2C_SR_TE | I2C_SR_RF)) != 0U) {
      prv_byte_done(i2c);
    }
  }

  if ((sr & I2C_SR_MSD) != 0U) {
    if (state->error == 0 && state->idx < state->num_msgs) {
      // A STOP the transfer did not ask for
      state->error = -EIO;
    }
    prv_complete(i2c, state->error);
  }
}

static int prv_start(const struct pbl_i2c_bus *bus, uint16_t addr, struct pbl_i2c_msg *msgs,
                     uint8_t num_msgs) {
  const struct pbl_i2c_sf32lb *i2c = prv_i2c(bus);
  struct pbl_i2c_sf32lb_state *state = i2c->state;

  if ((prv_getreg(i2c, SF32LB52_I2C_SR_OFFSET) & I2C_SR_UB) != 0U) {
    prv_reset(i2c);
  }

  *state = (struct pbl_i2c_sf32lb_state){
    .msgs = msgs,
    .num_msgs = num_msgs,
    .addr = addr,
    .sleep_blocked = true,
  };
  soc_sf32lb_sleep_block(SOC_SF32LB_DEEPSLEEP);

  prv_putreg(i2c, SF32LB52_I2C_SR_OFFSET, prv_getreg(i2c, SF32LB52_I2C_SR_OFFSET));
  prv_send_addr(i2c);
  prv_putreg(i2c, SF32LB52_I2C_IER_OFFSET, IER_PIO);

  return 0;
}

static void prv_abort(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_sf32lb *i2c = prv_i2c(bus);

  prv_putreg(i2c, SF32LB52_I2C_IER_OFFSET, 0U);
  if (i2c->state->dma) {
    prv_dma_finish(i2c);
  }
  prv_reset(i2c);
  prv_sleep_allow(i2c);
  pbl_irq_clear_pending(s_instances[i2c->index - 1U].irqn);
}

static void prv_enable(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_sf32lb *i2c = prv_i2c(bus);
  struct pbl_clk clk = prv_clk(i2c);

  (void)pbl_clk_enable(&clk);
  prv_configure(i2c);
}

static void prv_disable(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_sf32lb *i2c = prv_i2c(bus);
  struct pbl_clk clk = prv_clk(i2c);

  prv_putreg(i2c, SF32LB52_I2C_IER_OFFSET, 0U);
  pbl_modifyl(I2C_CR_IUE, 0U, prv_base(i2c) + SF32LB52_I2C_CR_OFFSET);
  (void)pbl_clk_disable(&clk);
}

static int prv_init(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_sf32lb *i2c = prv_i2c(bus);
  int ret;

  if (i2c->index == 0U || i2c->index > ARRAY_LENGTH(s_instances) || i2c->clock_hz == 0U) {
    return -EINVAL;
  }

  ret = pbl_device_init(&SF32LB_HPSYS_RCC->dev);
  if (ret != 0) {
    return ret;
  }

  if (i2c->dma != nullptr) {
    const struct pbl_dma_slave_config cfg = {
      .src_addr = prv_base(i2c) + SF32LB52_I2C_FIFO_OFFSET,
      .src_addr_width = PBL_DMA_SLAVE_BUSWIDTH_1_BYTE,
    };

    PBL_ASSERTN(dcache_line_size() <= DCACHE_LINE_SIZE_MAX);
    ret = pbl_device_init(&i2c->dma->ctrl->dev);
    if (ret != 0) {
      return ret;
    }

    ret = pbl_dma_slave_config(i2c->dma, &cfg);
    if (ret != 0) {
      return ret;
    }
  }

  ret = pbl_pinctrl_select_state(&i2c->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(s_instances[i2c->index - 1U].irqn);

  return 0;
}

const struct pbl_i2c_bus_ops pbl_i2c_sf32lb_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .start = prv_start,
  .abort = prv_abort,
};
