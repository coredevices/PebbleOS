/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/dis.h>

int pbl_bt__services_init(const struct pbl_bt_dis_info *dis_info);

void pbl_bt__bas_init(void);
void pbl_bt__dis_init(const struct pbl_bt_dis_info *info);
int pbl_bt__ppog_reversed_init(void);
int pbl_bt__pps_init(void);
