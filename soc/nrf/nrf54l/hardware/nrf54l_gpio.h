/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define NRF54L_GPIO_OUTSET_OFFSET     0x0004
#define NRF54L_GPIO_OUTCLR_OFFSET     0x0008
#define NRF54L_GPIO_IN_OFFSET         0x000c
#define NRF54L_GPIO_PIN_CNF_OFFSET(n) (0x0080 + ((n) << 2))

/* Register bit definitions */

#define GPIO_PIN_CNF_DIR_OUTPUT       PBL_BIT(0)
#define GPIO_PIN_CNF_INPUT_DISCONNECT PBL_BIT(1)
#define GPIO_PIN_CNF_PULL_MASK        PBL_GENMASK(3, 2)
#define GPIO_PIN_CNF_PULL_DISABLED    0U
#define GPIO_PIN_CNF_PULL_DOWN        1U
#define GPIO_PIN_CNF_PULL_UP          3U
#define GPIO_PIN_CNF_DRIVE1_MASK      PBL_GENMASK(11, 10)
#define GPIO_PIN_CNF_DRIVE1_D1        2U
#define GPIO_PIN_CNF_SENSE_MASK       PBL_GENMASK(17, 16)
#define GPIO_PIN_CNF_SENSE_HIGH       2U
#define GPIO_PIN_CNF_SENSE_LOW        3U
