/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

struct pbl_thread;
struct posix_thread;

extern struct pbl_thread *pbl_cur;

//! Each kernel thread is a pthread that runs only while the kernel says so.
//! The pthread's state lives apart from the kernel thread, which can be
//! recreated while an aborted pthread still waits on its own state.
struct pbl_arch_thread {
  struct posix_thread *pt;
};

static inline struct pbl_thread *arch_cur_get(void) {
  return pbl_cur;
}
