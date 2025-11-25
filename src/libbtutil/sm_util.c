/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

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
