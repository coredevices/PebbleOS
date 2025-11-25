/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if !CAPABILITY_HAS_ROCKY_JS
#include "rockyjs/rocky_res.h"

bool rocky_event_loop_with_resource(uint32_t resource_id) {
  return false;
}

RockyResourceValidation rocky_app_validate_resources(const PebbleProcessMd *md) {
  // No resources to validate so just pretend they are valid
  return RockyResourceValidation_Valid;
}
#endif
