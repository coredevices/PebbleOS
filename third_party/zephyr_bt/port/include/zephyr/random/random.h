/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

void sys_rand_get(void *dst, size_t len);
int sys_csrand_get(void *dst, size_t len);

static inline uint32_t sys_rand32_get(void) {
  uint32_t v;
  sys_rand_get(&v, sizeof(v));
  return v;
}
