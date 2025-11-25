/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef enum {
  SpiPeriphClockAPB1,
  SpiPeriphClockAPB2
} SpiPeriphClock;

//! @internal
//! Get the nearest SPI prescaler. Updates bus_frequency with the actual frequency
//! @param bus_frequency the desired bus frequency
//! @param periph_clock The peripheral clock that is used.
uint16_t spi_find_prescaler(uint32_t bus_frequency, SpiPeriphClock periph_clock);
