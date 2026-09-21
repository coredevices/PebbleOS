/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

#define GATT_CLIENT_DISCOVERY_MAX_RETRY_BITS (2)
#define GATT_CLIENT_DISCOVERY_MAX_RETRY      ((1 << GATT_CLIENT_DISCOVERY_MAX_RETRY_BITS) - 1)

//! Starts discovery of all GATT services, characteristics and descriptors.
//! @param device The device of which its services, characteristics and
//! descriptors need to be discovered.
//! @return PBL_BT_ERRNO_OK If the discovery process was started successfully,
//! PBL_BT_ERRNO_INVALID_PARAMETER if the device was not connected,
//! PBL_BT_ERRNO_INVALID_STATE if service discovery was already on-going, or
//! an internal error otherwise (>= PBL_BT_ERRNO_INTERNAL_ERROR_BEGIN).
enum pbl_bt_errno gatt_client_discovery_discover_all(const struct pbl_bt_device_internal *device);
