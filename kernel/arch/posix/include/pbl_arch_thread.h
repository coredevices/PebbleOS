/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

struct posix_thread;

//! Each kernel thread is a pthread that runs only while the kernel says so.
//! The pthread's state lives apart from the kernel thread, which can be
//! recreated while an aborted pthread still waits on its own state.
struct pbl_arch_thread {
  struct posix_thread *pt;
};
