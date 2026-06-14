/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/bluetooth/ble_hrm.h"

#include "applib/event_service_client.h"
#include "comm/ble/gap_le_connection.h"
#include "comm/bt_lock.h"
#include "kernel/pbl_malloc.h"
#include "kernel/event_loop.h"
#include "kernel/events.h"
#include "process_management/app_manager.h"
#include "pbl/services/analytics/analytics.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/activity/activity.h"
#include "shell/system_app_ids.auto.h"
#include "system/logging.h"
#include "system/passert.h"

#include <bluetooth/gap_le_connect.h>
#include <bluetooth/hrm_service.h>
#include <btutil/bt_device.h>
#include <util/list.h>
#include <util/size.h>

#ifdef CONFIG_HRM

static bool s_ble_hrm_is_inited;
static int s_ble_hrm_subscription_count;
static struct {
  EventServiceInfo service_info;
  HRMSessionRef manager_session;
} s_ble_hrm_session;

static bool prv_hw_and_sw_supports_hrm(void) {
  return (bt_driver_is_hrm_service_supported() &&
          sys_hrm_manager_is_hrm_present());
}

bool ble_hrm_is_supported_and_enabled(void) {
  return (prv_hw_and_sw_supports_hrm() && activity_prefs_heart_rate_is_enabled());
}

static void prv_reset_subscriptions(void);

//! The HRM GATT service is exposed here, but the watch does not share heart
//! rate yet: BLE HRM sharing is introduced (gated to an active workout) in a
//! later change. Until then a subscriber receives nothing and is never
//! prompted.
static bool prv_is_sharing(const GAPLEConnection *const connection) {
  return false;
}

bool ble_hrm_is_sharing_to_connection(const GAPLEConnection *const connection) {
  bt_lock_assert_held(true);
  if (!connection) {
    return false;
  }
  return prv_is_sharing(connection);
}

bool ble_hrm_is_sharing(void) {
  return (s_ble_hrm_subscription_count > 0);
}

typedef struct {
  BTDeviceInternal *next_permitted_device;
  size_t slots_left;
} CopySharingDevicesCtx;

static void prv_copy_sharing_devices_for_each_connection_cb(GAPLEConnection *connection,
                                                              void *data) {
  CopySharingDevicesCtx *ctx = data;
  if (ctx->slots_left && prv_is_sharing(connection)) {
    *ctx->next_permitted_device = connection->device;
    ++ctx->next_permitted_device;
    --ctx->slots_left;
  }
}

static size_t prv_copy_sharing_devices(BTDeviceInternal *devices_out,
                                       size_t max_devices) {
  bt_lock();
  CopySharingDevicesCtx ctx = {
    .next_permitted_device = devices_out,
    .slots_left = max_devices,
  };
  gap_le_connection_for_each(prv_copy_sharing_devices_for_each_connection_cb, &ctx);
  bt_unlock();
  return (max_devices - ctx.slots_left);
}

static void prv_ble_hrm_handle_hrm_data(PebbleEvent *e, void *context) {
  if (!s_ble_hrm_is_inited) {
    return;
  }
  if (s_ble_hrm_subscription_count == 0) {
    return;
  }
  PBL_ASSERTN(e->type == PEBBLE_HRM_EVENT);
  const PebbleHRMEvent *const hrm_event = &e->hrm;
  if (hrm_event->event_type != HRMEvent_BPM) {
    return;
  }
  const BleHrmServiceMeasurement measurement = {
    .bpm = hrm_event->bpm.bpm,
    .is_on_wrist = (hrm_event->bpm.quality >= HRMQuality_Worst),
  };

  BTDeviceInternal sharing_to_devices[4];
  const size_t num_devices = prv_copy_sharing_devices(sharing_to_devices,
                                                      ARRAY_LENGTH(sharing_to_devices));
  bt_driver_hrm_service_handle_measurement(&measurement, sharing_to_devices, num_devices);
}

static void prv_start_hrm_kernel_main(void *unused) {
  PBL_LOG_INFO("BLE HRM sharing started");
  s_ble_hrm_session.service_info = (EventServiceInfo) {
    .type = PEBBLE_HRM_EVENT,
    .handler = prv_ble_hrm_handle_hrm_data,
  };
  event_service_client_subscribe(&s_ble_hrm_session.service_info);
  s_ble_hrm_session.manager_session =
      hrm_manager_subscribe_with_callback(INSTALL_ID_INVALID, 1 /*update_interval_s*/,
                                          0 /*expire_s*/, HRMFeature_BPM, NULL, NULL);
}

static void prv_stop_hrm_kernel_main(void *unused) {
  PBL_LOG_INFO("BLE HRM sharing stopped");
  sys_hrm_manager_unsubscribe(s_ble_hrm_session.manager_session);
  event_service_client_unsubscribe(&s_ble_hrm_session.service_info);
}

static void prv_execute_on_kernel_main(CallbackEventCallback cb) {
  if (pebble_task_get_current() != PebbleTask_KernelMain) {
    launcher_task_add_callback(cb, NULL);
  } else {
    cb(NULL);
  }
}

void ble_hrm_handle_activity_prefs_heart_rate_is_enabled(bool is_enabled) {
  if (!prv_hw_and_sw_supports_hrm()) {
    return;
  }
  PBL_LOG_INFO("BLE HRM heart rate pref updated: is_enabled=%u", is_enabled);

  if (!is_enabled) {
    prv_reset_subscriptions();
  }
  bt_driver_hrm_service_enable(is_enabled);
}

static void prv_put_sharing_state_updated_event(int subscription_count) {
  // 2 purposes of this event:
  // - refresh the Settings/Bluetooth UI whenever a device (un)subscribes.
  // - present a "Sharing HRM" icon in the Settings app glance.
  PebbleEvent e = {
    .type = PEBBLE_BLE_HRM_SHARING_STATE_UPDATED_EVENT,
    .bluetooth = {
      .le = {
        .hrm_sharing_state = {
          .subscription_count = subscription_count,
        },
      },
    },
  };
  event_put(&e);
}

static void prv_update_is_sharing(GAPLEConnection *connection, bool prev_is_sharing) {
  const bool is_sharing = prv_is_sharing(connection);
  if (is_sharing == prev_is_sharing) {
    return;
  }

  if (is_sharing) {
    if (s_ble_hrm_subscription_count == 0) {
      prv_execute_on_kernel_main(prv_start_hrm_kernel_main);
    }
    ++s_ble_hrm_subscription_count;
  } else {
    --s_ble_hrm_subscription_count;
    if (s_ble_hrm_subscription_count == 0) {
      prv_execute_on_kernel_main(prv_stop_hrm_kernel_main);
    }
  }

  // Emit for every subscription, so Settings/Bluetooth menu can update accordingly.
  prv_put_sharing_state_updated_event(s_ble_hrm_subscription_count);
}

static void prv_update_subscription(GAPLEConnection *connection, bool is_subscribed) {
  bt_lock_assert_held(true);
  if (connection->hrm_service_is_subscribed == is_subscribed) {
    return;
  }
  PBL_LOG_INFO("BLE HRM sharing: conn <%p> is_subscribed=%u", connection, is_subscribed);

  const bool prev_is_sharing = prv_is_sharing(connection);
  connection->hrm_service_is_subscribed = is_subscribed;
  prv_update_is_sharing(connection, prev_is_sharing);
}

static void prv_reset_subscriptions(void) {
  bt_lock();
  if (s_ble_hrm_subscription_count) {
    s_ble_hrm_subscription_count = 0;
    bt_unlock();

    prv_execute_on_kernel_main(prv_stop_hrm_kernel_main);
  } else {
    bt_unlock();
  }
}

void bt_driver_cb_hrm_service_update_subscription(const BTDeviceInternal *device,
                                                  bool is_subscribed) {
  bt_lock();
  if (!s_ble_hrm_is_inited) {
    goto unlock;
  }
  GAPLEConnection *connection = gap_le_connection_by_device(device);
  if (!connection) {
    PBL_LOG_ERR("Subscription update but no connection?");
    goto unlock;
  }
  prv_update_subscription(connection, is_subscribed);
unlock:
  bt_unlock();
}

void ble_hrm_handle_disconnection(GAPLEConnection *connection) {
  if (!s_ble_hrm_is_inited) {
    return;
  }
  prv_update_subscription(connection, false /* is_subscribed */);
}

void ble_hrm_init(void) {
  s_ble_hrm_is_inited = true;
}

void ble_hrm_deinit(void) {
  s_ble_hrm_is_inited = false;
  prv_reset_subscriptions();
}

#endif
