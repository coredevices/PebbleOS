/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/util/attributes.h"
#include "pbl/util/build_id.h"

bool WEAK build_id_contains_gnu_build_id(const ElfExternalNote *note) {
  return false;
}
