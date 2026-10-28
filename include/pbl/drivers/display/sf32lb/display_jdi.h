/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/pwm.h>

#include <bf0_hal.h>
#include <bf0_hal_lcdc.h>
#include <board/board.h>

/**
 * @defgroup drivers_display_sf32lb SF32LB JDI display
 * @ingroup drivers_display
 * @brief @ref drivers_display implementation for JDI memory-in-pixel panels driven by the
 * SF32LB LCD controller.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
typedef struct DisplayJDIState {
  LCDC_HandleTypeDef hlcdc;
} DisplayJDIState;
/** @endcond */

/** @brief Splash image. */
typedef struct DisplayJDISplash {
  /** Image data. */
  const uint8_t *data;
  /** Width in pixels. */
  uint16_t width;
  /** Height in pixels. */
  uint16_t height;
} DisplayJDISplash;

/** @brief JDI display device. */
typedef const struct DisplayJDIDevice {
  /** Driver runtime state. */
  DisplayJDIState *state;
  /** LCD controller interrupt. */
  IRQn_Type irqn;
  /** VCOM inversion signal, run at its period with a 50% duty cycle while the panel is on. */
  struct pbl_pwm vcom;
  /** Panel interface pins. */
  struct pbl_pinctrl pinctrl;
  /** Panel inputs driven low while the panel is power cycled, as GPIOs. */
  const struct pbl_gpio *discharge;
  /** Number of @ref discharge pins. */
  uint8_t num_discharge;
  /** Panel VDDP supply enable. */
  struct pbl_gpio vddp;
  /** Panel VLCD supply enable. */
  struct pbl_gpio vlcd;
  /** Splash image; not used by the driver. */
  DisplayJDISplash splash;
} DisplayJDIDevice;

/**
 * @brief LCD controller interrupt handler.
 *
 * @param disp Display device.
 */
void display_jdi_irq_handler(DisplayJDIDevice *disp);

#ifndef CONFIG_RELEASE
/**
 * @brief Drop the next LCD transfer-complete interrupt.
 *
 * Test hook simulating a lost completion. The silent-loss timer then fires about 500 ms later
 * and crashes the system, producing a coredump. Not available in release builds.
 */
void display_jdi_test_drop_next_complete(void);
#endif

/** @} */
