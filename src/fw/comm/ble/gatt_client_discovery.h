/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>

#define GATT_CLIENT_DISCOVERY_MAX_RETRY_BITS (2)
#define GATT_CLIENT_DISCOVERY_MAX_RETRY ((1 << GATT_CLIENT_DISCOVERY_MAX_RETRY_BITS) - 1)

//! Starts discovery of all GATT services, characteristics and descriptors.
//! @param device The device of which its services, characteristics and
//! descriptors need to be discovered.
//! @return BTErrnoOK If the discovery process was started successfully,
//! BTErrnoInvalidParameter if the device was not connected,
//! BTErrnoInvalidState if service discovery was already on-going, or
//! an internal error otherwise (>= BTErrnoInternalErrorBegin).
BTErrno gatt_client_discovery_discover_all(const BTDeviceInternal *device);
