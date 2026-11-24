/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pressure_service.h"

#include "syscall/syscall.h"

bool pressure_service_peek(uint32_t *pressure_pa) {
  if (pressure_pa == nullptr) {
    return false;
  }

  return sys_pressure_read(pressure_pa) == 0;
}
