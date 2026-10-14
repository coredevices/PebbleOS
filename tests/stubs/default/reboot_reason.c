/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <system/reboot_reason.h>

PBL_WEAK void reboot_reason_set(RebootReason *reason) {
}
