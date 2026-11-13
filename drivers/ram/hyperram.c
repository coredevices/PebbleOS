/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/hyperbus.h>
#include <pbl/drivers/ram/hyperram.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

// Register space, in words
#define REG_ID0 0x000U
#define REG_CR0 0x800U
#define REG_CR1 0x801U

#define ID0_MANUFACTURER_MASK PBL_GENMASK(3, 0)
#define ID0_COL_BITS_MASK     PBL_GENMASK(7, 4)
#define ID0_ROW_BITS_MASK     PBL_GENMASK(12, 8)

// Clear to enter deep power-down
#define CR0_NORMAL       PBL_BIT(15)
#define CR0_RESERVED     0x0f00U
#define CR0_LATENCY_MASK PBL_GENMASK(7, 4)
#define CR0_WRAP_LEGACY  PBL_BIT(2)
#define CR0_BURST_32     0x3U

#define CR1_HYBRID_SLEEP PBL_BIT(5)

// From the supply or a reset to the first access
#define POWER_UP_US 150U
// From the wake-up pulse to the first access, out of hybrid sleep or deep power-down
#define EXIT_US 150U
// The memory enters hybrid sleep after this, even if the CPU does not get to sleep
#define HYBRID_SLEEP_US 150U
// Margin for the memory to enter deep power-down before the controller is gated
#define DPD_SETTLE_US 100U

#define CS_LOW_MAX_NS    4000U
#define CS_HIGH_MIN_NS   15U
#define READ_RECOVERY_NS 60U

// Fastest clock of each latency
static const struct {
  uint32_t max_hz;
  uint8_t clocks;
} s_latencies[] = {
  {85000000U, 3U}, {104000000U, 4U}, {133000000U, 5U}, {166000000U, 6U}, {200000000U, 7U},
};

static const struct pbl_ram_hyperram *prv_ram(const struct pbl_ram *ram) {
  return container_of(ram, const struct pbl_ram_hyperram, ram);
}

static int prv_update(const struct pbl_hyperbus *bus, uint32_t reg, uint16_t clear, uint16_t set) {
  uint16_t val;
  int ret;

  ret = pbl_hyperbus_read_reg(bus, reg, &val);
  if (ret != 0) {
    return ret;
  }

  return pbl_hyperbus_write_reg(bus, reg, (val & ~clear) | set);
}

static int prv_latency(const struct pbl_hyperbus *bus, uint8_t *clocks) {
  uint32_t rate_hz;
  int ret;

  ret = pbl_hyperbus_get_rate(bus, &rate_hz);
  if (ret != 0) {
    return ret;
  }

  for (size_t i = 0U; i < ARRAY_LENGTH(s_latencies); i++) {
    if (rate_hz <= s_latencies[i].max_hz) {
      *clocks = s_latencies[i].clocks;
      return 0;
    }
  }

  return -ERANGE;
}

// Wakes the memory up, whatever state it is in, and sets it and its bus up: deep power-down
// resets its configuration
static int prv_bring_up(const struct pbl_ram *ram) {
  const struct pbl_hyperbus *bus = prv_ram(ram)->bus;
  struct pbl_hyperbus_config cfg = {
    .cs_low_max_ns = CS_LOW_MAX_NS,
    .cs_high_min_ns = CS_HIGH_MIN_NS,
    .read_recovery_ns = READ_RECOVERY_NS,
  };
  int ret;

  ret = pbl_hyperbus_wake(bus);
  if (ret != 0) {
    return ret;
  }
  pbl_busy_wait_us(EXIT_US);

  ret = prv_latency(bus, &cfg.latency);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_hyperbus_configure(bus, &cfg, &ram->state->base);
  if (ret != 0) {
    return ret;
  }

  // The latency code is 0 for 5 clocks, then up by one per clock, wrapping
  return pbl_hyperbus_write_reg(bus, REG_CR0,
                                CR0_NORMAL | CR0_RESERVED |
                                    PBL_FIELD_PREP(CR0_LATENCY_MASK, (cfg.latency - 5U) & 0xfU) |
                                    CR0_WRAP_LEGACY | CR0_BURST_32);
}

static int prv_power_on(const struct pbl_ram *ram) {
  const struct pbl_hyperbus *bus = prv_ram(ram)->bus;
  int ret;

  ret = pbl_hyperbus_enable(bus);
  if (ret != 0) {
    return ret;
  }

  ret = prv_bring_up(ram);
  if (ret != 0) {
    pbl_hyperbus_disable(bus);
  }

  return ret;
}

// These run with interrupts disabled, where a memory that stops answering can only be fatal

static void prv_power_off(const struct pbl_ram *ram) {
  const struct pbl_hyperbus *bus = prv_ram(ram)->bus;
  const int ret = prv_update(bus, REG_CR0, CR0_NORMAL, 0U);

  PBL_ASSERT(ret == 0, "HyperRAM deep power-down failed (%d)", ret);
  pbl_busy_wait_us(DPD_SETTLE_US);
  pbl_hyperbus_disable(bus);
}

static void prv_retain(const struct pbl_ram *ram) {
  const int ret = prv_update(prv_ram(ram)->bus, REG_CR1, 0U, CR1_HYBRID_SLEEP);

  PBL_ASSERT(ret == 0, "HyperRAM hybrid sleep failed (%d)", ret);
  pbl_busy_wait_us(HYBRID_SLEEP_US);
}

static void prv_resume(const struct pbl_ram *ram) {
  const int ret = pbl_hyperbus_wake(prv_ram(ram)->bus);

  PBL_ASSERT(ret == 0, "HyperRAM wake-up failed (%d)", ret);
  pbl_busy_wait_us(EXIT_US);
}

// A register read only completes once the memory has taken the writes before it
static void prv_quiesce(const struct pbl_ram *ram) {
  uint16_t val;
  const int ret = pbl_hyperbus_read_reg(prv_ram(ram)->bus, REG_CR0, &val);

  PBL_ASSERT(ret == 0, "HyperRAM stopped answering (%d)", ret);
}

static int prv_init(const struct pbl_ram *ram) {
  const struct pbl_ram_hyperram *hram = prv_ram(ram);
  uint16_t id0;
  int ret;

  if (hram->vcc != nullptr) {
    ret = pbl_regulator_enable(hram->vcc);
    if (ret != 0) {
      return ret;
    }
  }
  // It may just have been powered
  pbl_busy_wait_us(POWER_UP_US);

  ret = prv_power_on(ram);
  if (ret != 0) {
    goto err_vcc;
  }

  ret = pbl_hyperbus_read_reg(hram->bus, REG_ID0, &id0);
  if (ret == 0 && (id0 == 0U || id0 == UINT16_MAX)) {
    ret = -ENODEV;
  }
  if (ret != 0) {
    pbl_hyperbus_disable(hram->bus);
    goto err_vcc;
  }
  // Words of two bytes, addressed by rows and columns
  ram->state->size = (size_t)2U << (PBL_FIELD_GET(ID0_ROW_BITS_MASK, id0) + 1U +
                                    PBL_FIELD_GET(ID0_COL_BITS_MASK, id0) + 1U);

  // Off until it is used
  prv_power_off(ram);

  return 0;

err_vcc:
  if (hram->vcc != nullptr) {
    (void)pbl_regulator_disable(hram->vcc);
  }

  return ret;
}

const struct pbl_ram_ops pbl_ram_hyperram_ops = {
  .init = prv_init,
  .power_on = prv_power_on,
  .power_off = prv_power_off,
  .retain = prv_retain,
  .resume = prv_resume,
  .quiesce = prv_quiesce,
};

#include PBL_DT_INSTANCES(cypress_hyperram)
