/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <board/board.h>

#include <bf0_hal.h>
#include <bf0_hal_lcdc.h>

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
  /** VCOM inversion signal generation. */
  struct {
    /** Low-power timer generating the signal. */
    LPTIM_TypeDef *lptim;
    /** Signal frequency in Hz. */
    uint8_t freq_hz;
  } vcom;
  /** Panel interface pins. */
  struct {
    /** Reset (XRST). */
    Pinmux xrst;
    /** Vertical start (VST). */
    Pinmux vst;
    /** Vertical clock (VCK). */
    Pinmux vck;
    /** Write enable (ENB). */
    Pinmux enb;
    /** Horizontal start (HST). */
    Pinmux hst;
    /** Horizontal clock (HCK). */
    Pinmux hck;
    /** Red data, odd pixels. */
    Pinmux r1;
    /** Red data, even pixels. */
    Pinmux r2;
    /** Green data, odd pixels. */
    Pinmux g1;
    /** Green data, even pixels. */
    Pinmux g2;
    /** Blue data, odd pixels. */
    Pinmux b1;
    /** Blue data, even pixels. */
    Pinmux b2;
    /** VCOM / FRP signal. */
    Pinmux vcom_frp;
    /** Inverted FRP signal (XFRP). */
    Pinmux xfrp;
  } pinmux;
  /** Panel VDDP supply enable. */
  OutputConfig vddp;
  /** Panel VLCD supply enable. */
  OutputConfig vlcd;
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
