/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

/* Mutex channels: a read takes the mutex if free, writing EX releases it. */
#define SF32LB52_MAILBOX_C1EXR_OFFSET 0x0014
#define SF32LB52_MAILBOX_C2EXR_OFFSET 0x002c

/* Register bit definitions */

#define MAILBOX_CxEXR_ID_MASK PBL_GENMASK(3, 0)
#define MAILBOX_CxEXR_EX      PBL_BIT(31)
