/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/nrf52_memorymap.h>

/* Register offsets */

#define NRF52_GPIO_OUT_OFFSET        0x0504
#define NRF52_GPIO_OUTSET_OFFSET     0x0508
#define NRF52_GPIO_OUTCLR_OFFSET     0x050c
#define NRF52_GPIO_IN_OFFSET         0x0510
#define NRF52_GPIO_DIR_OFFSET        0x0514
#define NRF52_GPIO_DIRSET_OFFSET     0x0518
#define NRF52_GPIO_DIRCLR_OFFSET     0x051c
#define NRF52_GPIO_PIN_CNF_OFFSET(n) (0x0700 + ((n) << 2))

/* Register bit definitions */

#define GPIO_PIN_CNF_DIR_OUTPUT       PBL_BIT(0)
#define GPIO_PIN_CNF_INPUT_DISCONNECT PBL_BIT(1)
#define GPIO_PIN_CNF_PULL_MASK        PBL_GENMASK(3, 2)
#define GPIO_PIN_CNF_PULL_DISABLED    0U
#define GPIO_PIN_CNF_PULL_DOWN        1U
#define GPIO_PIN_CNF_PULL_UP          3U
#define GPIO_PIN_CNF_DRIVE_MASK       PBL_GENMASK(10, 8)
#define GPIO_PIN_CNF_DRIVE_S0S1       0U
#define GPIO_PIN_CNF_DRIVE_S0D1       6U
#define GPIO_PIN_CNF_SENSE_MASK       PBL_GENMASK(17, 16)
