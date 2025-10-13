/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
