/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "preferred_content_size.h"

#include <pbl/kernel/syscall.h>

#include <shell/system_theme.h>

PBL_SYSCALL(PreferredContentSize, preferred_content_size) {
  return system_theme_get_content_size();
}
