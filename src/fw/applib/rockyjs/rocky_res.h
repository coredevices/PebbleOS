/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"
#include "resource/resource.h"

// True, if there's any resource with a compatible snapshot
bool rocky_app_has_compatible_bytecode_res(ResAppNum app_num);

typedef enum {
  RockyResourceValidation_NotRocky,
  RockyResourceValidation_Invalid,
  RockyResourceValidation_Valid,
} RockyResourceValidation;

// True if md describes rocky app and resources contain invalid bytecode
RockyResourceValidation rocky_app_validate_resources(const PebbleProcessMd *md);
