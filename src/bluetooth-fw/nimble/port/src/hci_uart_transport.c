/*
 * Copyright 2025 Google LLC
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

// TODO: transport.h needs os_mbuf.h to be included first
// clang-format off
#include "os/os_mbuf.h"
#include "nimble/transport.h"
// clang-format on

void ble_transport_ll_init(void) {}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om) {
  os_mbuf_free(om);
  return 0;
}

int ble_transport_to_ll_cmd_impl(void *buf) {
  ble_transport_free(buf);
  return 0;
}
