/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/led.h>
#include <pbl/drivers/spi.h>

/**
 * @defgroup drivers_led_ws2812b WS2812B
 * @ingroup drivers_led
 * @brief Worldsemi WS2812B RGB LED on a SPI MOSI line.
 *
 * Each LED bit is sent as three SPI bits, 100 for a 0 and 110 for a 1, so the SPI clock is
 * between 2.105 MHz and 2.85 MHz, followed by a low line long enough for the LED to latch. The
 * LED is the first of its chain.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
// Green, red and blue bytes, 3 SPI bits per bit
#define WS2812B_DATA_LEN 9U
// Low line for over 280 us at up to 2.85 MHz
#define WS2812B_RESET_LEN 100U

struct pbl_ws2812b_state {
  uint8_t buf[WS2812B_DATA_LEN + WS2812B_RESET_LEN];
};
/** @endcond */

/** @brief WS2812B. */
struct pbl_ws2812b {
  /** LED, child of the SPI bus. */
  struct pbl_led led;
  /** SPI device. */
  struct pbl_spi_dev spi;
  /** Runtime state. */
  struct pbl_ws2812b_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_led_ops pbl_ws2812b_ops;
int pbl_ws2812b_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define a WS2812B.
 *
 * @param sym Symbol of the instance.
 * @param _bus SPI bus.
 * @param _cs Chip select index, an unconnected one.
 * @param _max_hz SPI clock, 2.105 MHz to 2.85 MHz.
 */
#define PBL_WS2812B_DEFINE(sym, _bus, _cs, _max_hz)                                        \
  PBL_DEVICE_STATE_DEFINE(sym);                                                            \
  static struct pbl_ws2812b_state sym##_ws2812b_state;                                     \
  PBL_DEVICE_INSTANCE(sym, struct pbl_ws2812b, struct pbl_led, led) {                      \
    .led = PBL_LED_INIT(sym, "ws2812b", pbl_ws2812b_init, &(_bus)->dev, &pbl_ws2812b_ops), \
    .spi = PBL_SPI_DEV(_bus, _cs, _max_hz, PBL_SPI_MODE_0), .state = &sym##_ws2812b_state, \
  }

/** @} */
