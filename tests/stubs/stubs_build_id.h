/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"
#include "pbl/util/build_id.h"

bool PBL_WEAK build_id_contains_gnu_build_id(const ElfExternalNote *note) {
  return false;
}
