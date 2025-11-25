/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_smartstrap.h"
#include "services/normal/accessory/smartstrap_connection.h"
#include "services/normal/accessory/smartstrap_profiles.h"

#include <stdint.h>

//! Sends a connection request to the smartstrap (should only be called from smartstrap_connection)
void smartstrap_link_control_connect(void);

//! Disconnects from the smartstrap
void smartstrap_link_control_disconnect(void);

//! Checks whether the specified profile is supported by the smartstrap which is connected.
bool smartstrap_link_control_is_profile_supported(SmartstrapProfile profile);
