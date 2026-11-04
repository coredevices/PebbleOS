/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/syscall.h>

#include <popups/mic_banner.h>
#include <syscall/syscall.h>

PBL_SYSCALL(bool, sys_mic_banner_get_accessory_area, (OUT, GRect *, area_out)) {
  return mic_banner_get_accessory_area(area_out);
}
