/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/dark_mode_service.h>

PBL_WEAK bool dark_mode_service_peek(void) {
  return false;
}
