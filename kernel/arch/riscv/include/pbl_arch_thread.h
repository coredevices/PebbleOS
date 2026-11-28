/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

struct pbl_thread;

extern struct pbl_thread *pbl_cur;

struct pbl_arch_thread {};

static inline struct pbl_thread *arch_cur_get(void) {
  return pbl_cur;
}
