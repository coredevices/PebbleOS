/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/intc.h>
#include <pbl/drivers/ppg.h>

/**
 * @defgroup drivers_ppg_gh3026 GH3026
 * @ingroup drivers_ppg
 * @brief Goodix GH3026 @ref drivers_ppg driver.
 *
 * The board supplies the register table that sets up the optical stack (slot timing, LED
 * selection, initial currents and gains) and the matching slot description. The table must keep
 * the slot sequence in slot order; the driver enables slots and receivers at run time.
 *
 * @code{.c}
 * static const struct pbl_gh3026_reg s_regs[] = {{0x0004, 0x001F}, ...};
 * static const struct pbl_ppg_slot s_slots[] = {{.led_mask = 0x1, .rx_mask = 0x3}, ...};
 *
 * PBL_GH3026_DEFINE(s_gh3026, &s_i2c4.bus, 0x14,
 *                   .irq = PBL_IRQ_SPEC(SF32LB_GPIO1_INTC, 44, PBL_IRQ_TYPE_EDGE_FALLING),
 *                   .irq_input = PBL_GPIO(SF32LB_GPIO1, 44, 0),
 *                   .reset = PBL_GPIO(&s_npm1300_gpio.port, 3, PBL_GPIO_ACTIVE_LOW),
 *                   .regs = s_regs, .num_regs = ARRAY_LENGTH(s_regs),
 *                   .slots = s_slots, .num_slots = ARRAY_LENGTH(s_slots));
 *
 * const struct pbl_ppg *const PPG = &s_gh3026.ppg;
 * @endcode
 * @{
 */

/** @brief Register write. */
struct pbl_gh3026_reg {
  /** Address. */
  uint16_t addr;
  /** Value. */
  uint16_t val;
};

/** @brief FIFO words read per I2C transfer. */
#define PBL_GH3026_READ_WORDS 64

/** @brief GH3026 configuration. */
struct pbl_gh3026_config {
  /** Interrupt line. */
  struct pbl_irq_spec irq;
  /** Interrupt line, as an input. */
  struct pbl_gpio irq_input;
  /** Reset line, optional. */
  struct pbl_gpio reset;
  /** Register table, written at init. */
  const struct pbl_gh3026_reg *regs;
  /** Number of registers in the table. */
  uint16_t num_regs;
  /** Slots set up by the table. */
  const struct pbl_ppg_slot *slots;
  /** Number of slots. */
  uint8_t num_slots;
};

/** @brief GH3026 driver state, owned by the driver. */
struct pbl_gh3026_state {
  /** Front end description. */
  struct pbl_ppg_info info;
  /** Slot timers set up by the table. */
  uint16_t slot_timers[PBL_PPG_MAX_SLOTS];
  /** Fastest slot rate, in hertz. */
  uint32_t base_rate_hz;
  /** Full scale of each LED driver, in microamperes. */
  uint32_t led_fs_ua[PBL_PPG_MAX_LED_DRIVERS];
  /** LED driver calibration code from the efuse. */
  int8_t led_ecode;
  /** Sample rate. */
  uint16_t sample_rate_hz;
  /** Samples per sampling period. */
  uint16_t period_samples;
  /** Read buffer. */
  uint8_t buf[PBL_GH3026_READ_WORDS * sizeof(uint32_t)];
  /** Decoded samples. */
  struct pbl_ppg_sample samples[PBL_GH3026_READ_WORDS];
};

/** @brief GH3026. */
struct pbl_gh3026 {
  /** Front end, child of the I2C bus. */
  struct pbl_ppg ppg;
  /** Driver state. */
  struct pbl_gh3026_state *state;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /** Configuration. */
  struct pbl_gh3026_config config;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_ppg_ops pbl_gh3026_ops;
/** @endcond */

/**
 * @brief Define a GH3026.
 *
 * @param sym Symbol of the instance; the front end is @c sym.ppg.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param ... struct pbl_gh3026_config designated initializers.
 */
#define PBL_GH3026_DEFINE(sym, _bus, _addr, ...)                                \
  PBL_PPG_STATE_DEFINE(sym);                                                    \
  static struct pbl_gh3026_state sym##_gh3026_state;                            \
  const struct pbl_gh3026 sym = {                                               \
    .ppg = PBL_PPG_INIT(sym, "gh3026", &(_bus)->dev, nullptr, &pbl_gh3026_ops), \
    .state = &sym##_gh3026_state,                                               \
    .i2c = PBL_I2C_DEV(_bus, _addr),                                            \
    .config = {__VA_ARGS__},                                                    \
  };                                                                            \
  PBL_DEVICE_REGISTER(sym, &sym.ppg.dev)

/** @} */
