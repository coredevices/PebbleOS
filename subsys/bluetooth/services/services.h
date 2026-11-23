/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/dis.h>

/* A 128-bit UUID written most significant byte first, in the order of the stack. */
#define BLE_UUID_SWIZZLE_(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15) \
  a15, a14, a13, a12, a11, a10, a9, a8, a7, a6, a5, a4, a3, a2, a1, a0
#define BLE_UUID_SWIZZLE(x) BLE_UUID_SWIZZLE_(x)

int pbl_bt__services_init(const struct pbl_bt_dis_info *dis_info);

void pbl_bt__bas_init(void);
void pbl_bt__dis_init(const struct pbl_bt_dis_info *info);
int pbl_bt__gh3x2x_tuning_init(void);
int pbl_bt__ppog_reversed_init(void);
int pbl_bt__pps_init(void);
