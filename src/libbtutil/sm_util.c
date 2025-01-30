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

#include "sm_util.h"
#include "bt_device.h"

#include <bluetooth/sm_types.h>

#include <stdbool.h>
#include <string.h>

// -------------------------------------------------------------------------------------------------
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
bool sm_is_pairing_info_equal_identity(const SMPairingInfo *a, const SMPairingInfo *b) {
  return (a->is_remote_identity_info_valid &&
          b->is_remote_identity_info_valid &&
          bt_device_equal(&a->identity.opaque, &b->identity.opaque) &&
          memcmp(&a->irk, &b->irk, sizeof(SMIdentityResolvingKey)) == 0);
}
#pragma GCC diagnostic pop

// -------------------------------------------------------------------------------------------------
bool sm_is_pairing_info_empty(const SMPairingInfo *p) {
  return (!p->is_local_encryption_info_valid &&
          !p->is_remote_encryption_info_valid &&
          !p->is_remote_identity_info_valid &&
          !p->is_remote_signing_info_valid);
}

bool sm_is_pairing_info_irk_not_used(const SMIdentityResolvingKey *irk_key) {
  // Per BLE spec v4.2 section 10.7 "Privacy Feature":
  //
  // "The local or peer’s IRK shall be an all-zero key, if not applicable for the particular
  //  device identity."
  const SMIdentityResolvingKey empty_key = { };
  return (memcmp(irk_key, &empty_key, sizeof(empty_key)) == 0);
}
