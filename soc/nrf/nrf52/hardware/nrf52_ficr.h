/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <hardware/nrf52_memorymap.h>

/* Register offsets */

#define NRF52_FICR_DEVICEID_OFFSET(n) (0x0060 + (n) * 4)
/* Undocumented: part family and silicon revision */
#define NRF52_FICR_PART_OFFSET     0x0130
#define NRF52_FICR_REVISION_OFFSET 0x0134

/* Register bit definitions */

#define FICR_PART_NRF52840 0x08U

#define FICR_REVISION_HARDENED_APPROTECT 0x05U
