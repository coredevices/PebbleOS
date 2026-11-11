/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board.h>

#ifdef CONFIG_HRM
#include <pbl/drivers/hrm/qemu.h>
#endif

#ifdef CONFIG_HRM
static HRMDeviceState s_hrm_state;
static HRMDevice s_hrm = {
  .state = &s_hrm_state,
};
HRMDevice *const HRM = &s_hrm;
#endif
