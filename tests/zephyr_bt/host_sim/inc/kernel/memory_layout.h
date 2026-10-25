/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Stand-in for the firmware header, with what ctl/zbt_ctl.c uses.
#pragma once
#include <pbl/mcu/mpu.h>
const MpuRegion *memory_layout_get_app_region(void);
const MpuRegion *memory_layout_get_worker_region(void);
