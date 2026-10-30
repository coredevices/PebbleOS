/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "display.h"

#include <stdint.h>

#include <pbl/input/input.h>

struct pbl_uart;
typedef const struct HRMDevice HRMDevice;
typedef const struct MicDevice MicDevice;
typedef const struct AudioDevice AudioDevice;

void board_early_init(void);
void board_init(void);

#include "board_definitions.h"
