/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/nrf52_memorymap.h>

#define NRF52_PPI_NUM_CHANNELS 20U

/* Register offsets */

#define NRF52_PPI_CHENSET_OFFSET     0x0504
#define NRF52_PPI_CHENCLR_OFFSET     0x0508
#define NRF52_PPI_CH_EEP_OFFSET(n)   (0x0510 + ((n) << 3))
#define NRF52_PPI_CH_TEP_OFFSET(n)   (0x0514 + ((n) << 3))
#define NRF52_PPI_FORK_TEP_OFFSET(n) (0x0910 + ((n) << 2))
