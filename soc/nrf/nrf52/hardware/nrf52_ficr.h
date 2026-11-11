/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <hardware/nrf52_memorymap.h>

/* Register offsets */

#define NRF52_FICR_DEVICEID_OFFSET(n) (0x0060 + (n) * 4)
