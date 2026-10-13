/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/thread.h>

PBL_WEAK void psleep(int millis) {
}

PBL_WEAK void pbl_thread_sleep(pbl_timeout_t timeout) {
}
