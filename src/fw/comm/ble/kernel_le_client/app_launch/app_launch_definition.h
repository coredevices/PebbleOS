/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "app_launch.h"

#include <bluetooth/pebble_bt.h>

static const Uuid s_app_launch_service_uuid = {
  PEBBLE_BT_UUID_EXPAND(PEBBLE_BT_APP_LAUNCH_SERVICE_UUID_32BIT)
};

static const Uuid s_app_launch_characteristic_uuids[AppLaunchCharacteristicNum] = {
  [AppLaunchCharacteristicAppLaunch] = {
    PEBBLE_BT_UUID_EXPAND(PEBBLE_BT_APP_LAUNCH_CHARACTERISTIC_UUID_32BIT),
  },
};
