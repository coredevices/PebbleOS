/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/i2c/stm32.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/st/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/units.h>

#include <hardware/stm32_i2c.h>

// Most bytes a single NBYTES count covers
#define NBYTES_MAX 255U

// Prescaled clock of the timing, at most this rate
#define TICK_MAX_HZ 8000000U

#define CR1_IRQS \
  (I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_NACKIE | I2C_CR1_STOPIE | I2C_CR1_TCIE | I2C_CR1_ERRIE)

static const struct pbl_i2c_stm32 *prv_i2c(const struct pbl_i2c_bus *bus) {
  return PBL_CONTAINER_OF(bus, const struct pbl_i2c_stm32, bus);
}

static inline uint32_t prv_getreg(const struct pbl_i2c_stm32 *i2c, uint32_t offset) {
  return pbl_readl(i2c->base + offset);
}

static inline void prv_putreg(const struct pbl_i2c_stm32 *i2c, uint32_t offset, uint32_t value) {
  pbl_writel(value, i2c->base + offset);
}

static uint32_t prv_ticks(uint32_t ns, uint32_t tick_hz) {
  return (uint32_t)(((uint64_t)ns * tick_hz + 999999999U) / 1000000000U);
}

// SCL low and high times within the period, no shorter than the minimums of the bus mode
static int prv_timing(uint32_t ker_hz, uint32_t bus_hz, uint32_t *timingr) {
  const bool fast = bus_hz > 100000U;
  uint32_t presc = (ker_hz + TICK_MAX_HZ - 1U) / TICK_MAX_HZ - 1U;
  uint32_t tick_hz;
  uint32_t period;
  uint32_t low;
  uint32_t high;
  uint32_t scldel;

  if (presc > 15U) {
    return -EINVAL;
  }

  tick_hz = ker_hz / (presc + 1U);
  period = tick_hz / bus_hz;
  low = PBL_MAX(prv_ticks(fast ? 1300U : 4700U, tick_hz), period / 2U + 1U);
  high = PBL_MAX(prv_ticks(fast ? 600U : 4000U, tick_hz), period - PBL_MIN(period, low));
  scldel = PBL_MAX(prv_ticks(fast ? 100U : 250U, tick_hz), 1U);
  if (low > 256U || high > 256U || scldel > 16U) {
    return -EINVAL;
  }

  *timingr = PBL_FIELD_PREP(I2C_TIMINGR_PRESC_MASK, presc) |
             PBL_FIELD_PREP(I2C_TIMINGR_SCLDEL_MASK, scldel - 1U) |
             PBL_FIELD_PREP(I2C_TIMINGR_SDADEL_MASK, 1U) |
             PBL_FIELD_PREP(I2C_TIMINGR_SCLH_MASK, high - 1U) |
             PBL_FIELD_PREP(I2C_TIMINGR_SCLL_MASK, low - 1U);

  return 0;
}

// Messages joined by NOSTART go out as one segment, the others each after a repeated START
static void prv_segment(const struct pbl_i2c_stm32 *i2c, bool start) {
  struct pbl_i2c_stm32_state *st = i2c->state;
  uint32_t len = st->msgs[st->idx].len;
  uint8_t end = st->idx + 1U;

  while (end < st->num_msgs && (st->msgs[end].flags & PBL_I2C_MSG_NOSTART) != 0U) {
    len += st->msgs[end].len;
    end++;
  }

  st->seg_end = end;
  st->pos = 0U;
  st->left = len;
  st->moved = 0U;
  st->read = (st->msgs[st->idx].flags & PBL_I2C_MSG_READ) != 0U;

  prv_putreg(i2c, STM32_I2C_CR2_OFFSET,
             PBL_FIELD_PREP(I2C_CR2_SADD_MASK, (uint32_t)st->addr << 1) |
                 (st->read ? I2C_CR2_RD_WRN : 0U) |
                 PBL_FIELD_PREP(I2C_CR2_NBYTES_MASK, PBL_MIN(len, NBYTES_MAX)) |
                 (len > NBYTES_MAX ? I2C_CR2_RELOAD : 0U) | (start ? I2C_CR2_START : 0U));
}

// The byte of the segment the transfer is at, past the messages it finished
static uint8_t *prv_byte(struct pbl_i2c_stm32_state *st) {
  while (st->pos >= st->msgs[st->idx].len) {
    st->idx++;
    st->pos = 0U;
  }

  st->left--;
  st->moved++;

  return &st->msgs[st->idx].buf[st->pos++];
}

static void prv_stop_allow(const struct pbl_i2c_stm32 *i2c) {
  struct pbl_i2c_stm32_state *st = i2c->state;
  bool blocked;

  pbl_irq_lock();
  blocked = st->stop_blocked;
  st->stop_blocked = false;
  pbl_irq_unlock();

  if (blocked) {
    pbl_idle_state_release(SOC_STM32_IDLE_STOP);
  }
}

static void prv_complete(const struct pbl_i2c_stm32 *i2c, int status) {
  pbl_modifyl(CR1_IRQS, 0U, i2c->base + STM32_I2C_CR1_OFFSET);
  prv_stop_allow(i2c);
  pbl_i2c_bus_complete(&i2c->bus, status);
}

// A bus error leaves the controller to be reset, which disabling it does
static void prv_reset(const struct pbl_i2c_stm32 *i2c) {
  pbl_modifyl(I2C_CR1_PE, 0U, i2c->base + STM32_I2C_CR1_OFFSET);
  (void)prv_getreg(i2c, STM32_I2C_CR1_OFFSET);
  pbl_modifyl(0U, I2C_CR1_PE, i2c->base + STM32_I2C_CR1_OFFSET);
}

void pbl_i2c_stm32_irq_handler(const struct pbl_i2c_stm32 *i2c) {
  struct pbl_i2c_stm32_state *st = i2c->state;
  uint32_t isr = prv_getreg(i2c, STM32_I2C_ISR_OFFSET);

  if ((prv_getreg(i2c, STM32_I2C_CR1_OFFSET) & CR1_IRQS) == 0U) {
    return;
  }

  if ((isr & (I2C_ISR_BERR | I2C_ISR_ARLO | I2C_ISR_OVR)) != 0U) {
    prv_putreg(i2c, STM32_I2C_ICR_OFFSET, I2C_ICR_ALL);
    prv_reset(i2c);
    prv_complete(i2c, -EIO);
    return;
  }

  // The controller sends the STOP after a NACK, which ends the transfer below
  if ((isr & I2C_ISR_NACKF) != 0U) {
    prv_putreg(i2c, STM32_I2C_ICR_OFFSET, I2C_ISR_NACKF);
    if (st->error == 0) {
      st->error = st->moved == 0U ? -ENXIO : -EIO;
    }
  }

  if ((isr & I2C_ISR_TXIS) != 0U && !st->read && st->left > 0U) {
    prv_putreg(i2c, STM32_I2C_TXDR_OFFSET, *prv_byte(st));
  }

  if ((isr & I2C_ISR_RXNE) != 0U && st->read && st->left > 0U) {
    *prv_byte(st) = (uint8_t)prv_getreg(i2c, STM32_I2C_RXDR_OFFSET);
  }

  if ((isr & I2C_ISR_TCR) != 0U) {
    pbl_modifyl(I2C_CR2_NBYTES_MASK | I2C_CR2_RELOAD | I2C_CR2_START | I2C_CR2_STOP,
                PBL_FIELD_PREP(I2C_CR2_NBYTES_MASK, PBL_MIN(st->left, NBYTES_MAX)) |
                    (st->left > NBYTES_MAX ? I2C_CR2_RELOAD : 0U),
                i2c->base + STM32_I2C_CR2_OFFSET);
  } else if ((isr & I2C_ISR_TC) != 0U) {
    st->idx = st->seg_end;
    if (st->idx < st->num_msgs) {
      prv_segment(i2c, true);
    } else {
      pbl_modifyl(I2C_CR2_START, I2C_CR2_STOP, i2c->base + STM32_I2C_CR2_OFFSET);
    }
  }

  if ((isr & I2C_ISR_STOPF) != 0U) {
    prv_putreg(i2c, STM32_I2C_ICR_OFFSET, I2C_ISR_STOPF);
    // A NACK leaves the byte that was to follow in the transmit register
    prv_putreg(i2c, STM32_I2C_ISR_OFFSET, I2C_ISR_TXE);
    if (st->error == 0 && st->seg_end < st->num_msgs) {
      // A STOP the transfer did not ask for
      st->error = -EIO;
    }
    prv_complete(i2c, st->error);
  }
}

static int prv_start(const struct pbl_i2c_bus *bus, uint16_t addr, struct pbl_i2c_msg *msgs,
                     uint8_t num_msgs) {
  const struct pbl_i2c_stm32 *i2c = prv_i2c(bus);
  struct pbl_i2c_stm32_state *st = i2c->state;

  if ((prv_getreg(i2c, STM32_I2C_ISR_OFFSET) & I2C_ISR_BUSY) != 0U) {
    prv_reset(i2c);
  }

  st->msgs = msgs;
  st->num_msgs = num_msgs;
  st->idx = 0U;
  st->addr = addr;
  st->error = 0;
  st->stop_blocked = true;
  pbl_idle_state_block(SOC_STM32_IDLE_STOP);

  prv_putreg(i2c, STM32_I2C_ICR_OFFSET, I2C_ICR_ALL);
  pbl_modifyl(0U, CR1_IRQS, i2c->base + STM32_I2C_CR1_OFFSET);
  prv_segment(i2c, true);

  return 0;
}

static void prv_abort(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_stm32 *i2c = prv_i2c(bus);

  pbl_modifyl(CR1_IRQS, 0U, i2c->base + STM32_I2C_CR1_OFFSET);
  prv_reset(i2c);
  prv_putreg(i2c, STM32_I2C_ICR_OFFSET, I2C_ICR_ALL);
  prv_stop_allow(i2c);
}

static void prv_enable(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_stm32 *i2c = prv_i2c(bus);

  (void)pbl_clk_enable(&i2c->clk);
  prv_putreg(i2c, STM32_I2C_CR1_OFFSET, 0U);
  prv_putreg(i2c, STM32_I2C_TIMINGR_OFFSET, i2c->state->timingr);
  prv_putreg(i2c, STM32_I2C_CR1_OFFSET, I2C_CR1_PE);
}

static void prv_disable(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_stm32 *i2c = prv_i2c(bus);

  prv_putreg(i2c, STM32_I2C_CR1_OFFSET, 0U);
  (void)pbl_clk_disable(&i2c->clk);
}

static int prv_init(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_stm32 *i2c = prv_i2c(bus);
  uint64_t rate_mhz;
  int ret;

  if (i2c->clock_hz == 0U || i2c->clock_hz > 400000U) {
    return -EINVAL;
  }

  ret = pbl_device_init(&i2c->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_get_rate(&i2c->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  ret = prv_timing((uint32_t)(rate_mhz / PBL_MILLIHZ_PER_HZ), i2c->clock_hz, &i2c->state->timingr);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(&i2c->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(i2c->irq_ev);
  pbl_irq_enable(i2c->irq_er);

  return 0;
}

const struct pbl_i2c_bus_ops pbl_i2c_stm32_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .start = prv_start,
  .abort = prv_abort,
};

#include PBL_DT_INSTANCES(st_stm32f7_i2c)
