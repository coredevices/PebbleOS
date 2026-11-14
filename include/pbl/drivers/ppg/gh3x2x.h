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
 * @defgroup drivers_ppg_gh3x2x GH3X2X
 * @ingroup drivers_ppg
 * @brief Goodix GH3X2X @ref drivers_ppg driver.
 *
 * The driver sets the optical stack up with its own register table (slot timing, LED selection,
 * initial currents and gains): green and infrared LEDs on LED driver 0, red on LED driver 1, two
 * photodiodes. It enables slots and receivers at run time. pbl_gh3x2x_load_setup() loads another
 * table, as for factory tests.
 *
 * @code{.c}
 * PBL_GH3X2X_DEFINE(s_gh3x2x, &s_i2c4, 0x14,
 *                   .irq = PBL_IRQ_SPEC(SF32LB_GPIO1_INTC, 44, PBL_IRQ_TYPE_EDGE_FALLING),
 *                   .irq_input = PBL_GPIO(SF32LB_GPIO1, 44, 0),
 *                   .reset = PBL_GPIO(&s_npm1300_gpio, 3, PBL_GPIO_ACTIVE_LOW));
 * @endcode
 * @{
 */

/** @brief Register write. */
struct pbl_gh3x2x_reg {
  /** Address. */
  uint16_t addr;
  /** Value. */
  uint16_t val;
};

/** @brief Setup of the optical stack. The table must keep the slot sequence in slot order. */
struct pbl_gh3x2x_setup {
  /** Register table, written after a reset. */
  const struct pbl_gh3x2x_reg *regs;
  /** Number of registers in the table. */
  uint16_t num_regs;
  /** Slots set up by the table. */
  const struct pbl_ppg_slot *slots;
  /** Number of slots. */
  uint8_t num_slots;
};

/** @brief FIFO words read per I2C transfer. */
#define PBL_GH3X2X_READ_WORDS 64

/** @brief GH3X2X configuration. */
struct pbl_gh3x2x_config {
  /** Interrupt line. */
  struct pbl_irq_spec irq;
  /** Interrupt line, as an input. */
  struct pbl_gpio irq_input;
  /** Reset line, optional. */
  struct pbl_gpio reset;
};

/** @brief GH3X2X driver state, owned by the driver. */
struct pbl_gh3x2x_state {
  /** Front end description. */
  struct pbl_ppg_info info;
  /** Loaded setup. */
  const struct pbl_gh3x2x_setup *setup;
  /** Slot timers set up by the table. */
  uint16_t slot_timers[PBL_PPG_MAX_SLOTS];
  /** Fastest slot rate, in hertz. */
  uint32_t base_rate_hz;
  /** Full scale of each LED driver, in microamperes. */
  uint32_t led_fs_ua[PBL_PPG_MAX_LED_DRIVERS];
  /** LED driver calibration code from the efuse. */
  int8_t led_ecode;
  /** The chip is sampling. */
  bool sampling;
  /** The FIFO block is powered off. */
  bool fifo_off;
  /** Sample rate. */
  uint16_t sample_rate_hz;
  /** Samples per sampling period. */
  uint16_t period_samples;
  /** Read buffer. */
  uint8_t buf[PBL_GH3X2X_READ_WORDS * sizeof(uint32_t)];
  /** Decoded samples. */
  struct pbl_ppg_sample samples[PBL_GH3X2X_READ_WORDS];
};

/** @brief GH3X2X. */
struct pbl_gh3x2x {
  /** Front end, child of the I2C bus. */
  struct pbl_ppg ppg;
  /** Driver state. */
  struct pbl_gh3x2x_state *state;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
  /** Configuration. */
  struct pbl_gh3x2x_config config;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_ppg_ops pbl_gh3x2x_ops;
/** @endcond */

/**
 * @brief Define a GH3X2X.
 *
 * @param sym Symbol of the instance, a struct pbl_ppg.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 * @param ... Designated initializers of the configuration: @c irq, @c irq_input and @c reset.
 */
#define PBL_GH3X2X_DEFINE(sym, _bus, _addr, ...)                                            \
  PBL_PPG_STATE_DEFINE(sym);                                                                \
  static struct pbl_gh3x2x_state sym##_gh3x2x_state;                                        \
  PBL_DEVICE_INSTANCE(sym, struct pbl_gh3x2x, struct pbl_ppg, ppg) {                        \
    .ppg = PBL_PPG_INIT(sym, "gh3x2x", &(_bus)->dev, nullptr, &pbl_gh3x2x_ops),             \
    .state = &sym##_gh3x2x_state, .i2c = PBL_I2C_DEV(_bus, _addr), .config = {__VA_ARGS__}, \
  }

/**
 * @brief Reset the part and load another setup.
 *
 * The front end has to be stopped. Settings made through the class are lost.
 *
 * @param ppg GH3X2X front end.
 * @param setup Setup, or nullptr for the driver's.
 *
 * @retval 0 On success.
 * @retval -EINVAL The setup has too many slots.
 * @retval -errno Other negative errno on failure.
 */
int pbl_gh3x2x_load_setup(const struct pbl_ppg *ppg, const struct pbl_gh3x2x_setup *setup);

/** @} */
