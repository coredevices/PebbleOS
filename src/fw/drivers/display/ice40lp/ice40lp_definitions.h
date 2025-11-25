/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"

#include <stdbool.h>
#include <stdint.h>

// TODO: Calling this ICE40LP kind of sucks, but I can't think of anything better without doing a
// whole big display system refactor, so we're keeping it as ICE40LP.
typedef struct ICE40LPDeviceState {
} ICE40LPDeviceState;

typedef const struct ICE40LPDevice {
  ICE40LPDeviceState *state;

  SPISlavePort *spi_port;
  uint32_t base_spi_frequency;
  uint32_t fast_spi_frequency;

  const OutputConfig creset;
  const InputConfig cdone;
  const InputConfig busy;
  const ExtiConfig cdone_exti;
  const ExtiConfig busy_exti;

  bool use_6v6_rail;
} ICE40LPDevice;
