/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/uuid.h"


bool uuid_equal(const Uuid *uu1, const Uuid *uu2) {
  return false;
}

void uuid_generate(Uuid *uuid_out) {
}

bool uuid_is_system(const Uuid *uuid) {
  return false;
}

bool uuid_is_invalid(const Uuid *uuid) {
  return false;
}

void uuid_to_string(const Uuid *uuid, char *buffer) {

}
