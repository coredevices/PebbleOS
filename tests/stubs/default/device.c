/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/device.h>
#include <pbl/kernel/compiler.h>

PBL_WEAK bool pbl_device_is_ready(const struct pbl_device *dev) {
  return false;
}
