/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "stubs_bt_driver_gatt_client_discovery.h"

#include "fake_GATTAPI.h"
#include "comm/ble/gap_le_connection.h"
#include "comm/ble/gatt_client_discovery.h"

#include <bluetooth/gatt_discovery.h>
#include <kernel/pbl_malloc.h>

// TODO: Rethink how we want to stub out these new driver wrapper calls.

// Forward declarations of conversion functions from fake_GATTAPI_test_vectors.c
extern GATTService *fake_gatt_convert_discovery_indication_to_service(
    GATT_Service_Discovery_Indication_Data_t *indication_data);

// Callback function that processes GATT service discovery events
static void prv_gatt_discovery_event_callback(unsigned int stack_id,
                                               GATT_Service_Discovery_Event_Data_t *event,
                                               unsigned long callback_param) {
  // Get the connection from the callback parameter (connection ID)
  GAPLEConnection *connection = gap_le_connection_by_gatt_id((unsigned int)callback_param);
  if (!connection) {
    return;
  }

  if (event->Event_Data_Type == 1 /* etGATT_Service_Discovery_Indication */) {
    GATT_Service_Discovery_Indication_Data_t *indication_data =
        event->Event_Data.GATT_Service_Discovery_Indication_Data;
    if (indication_data) {
      // Convert the Bluetopia indication to GATTService*
      GATTService *service = fake_gatt_convert_discovery_indication_to_service(indication_data);
      if (service) {
        bt_driver_cb_gatt_client_discovery_handle_indication(connection, service, BTErrnoOK);
      }
      // If conversion fails, do nothing - the service won't be added
    }
  } else if (event->Event_Data_Type == 0 /* etGATT_Service_Discovery_Complete */) {
    GATT_Service_Discovery_Complete_Data_t *complete_data =
        event->Event_Data.GATT_Service_Discovery_Complete_Data;
    BTErrno error = BTErrnoOK;
    if (complete_data && complete_data->Status != 0) {
      error = BTErrnoWithBluetopiaError(complete_data->Status);
    }
    bt_driver_cb_gatt_client_discovery_complete(connection, error);
  }
}

BTErrno bt_driver_gatt_start_discovery_range(const GAPLEConnection *connection, const ATTHandleRange *data) {
  // Call the fake GATT API so the test can properly track discovery state
  GATT_Attribute_Handle_Group_t range = {
    .Starting_Handle = data->start,
    .Ending_Handle = data->end,
  };
  // Pass the GATT connection ID as the callback parameter so we can retrieve the connection later
  int ret = GATT_Start_Service_Discovery_Handle_Range(0, connection->gatt_connection_id, &range, 0,
                                                      NULL, prv_gatt_discovery_event_callback,
                                                      connection->gatt_connection_id);
  return (ret == 0) ? BTErrnoOK : BTErrnoInternalErrorBegin;
}

BTErrno bt_driver_gatt_stop_discovery(GAPLEConnection *connection) {
  // Call the fake GATT API so the test can properly track discovery state
  GATT_Stop_Service_Discovery(0, 0);
  return BTErrnoOK;
}

void bt_driver_gatt_handle_finalize_discovery(GAPLEConnection *connection) {
}

void bt_driver_gatt_handle_discovery_abandoned(void) {
}

uint32_t bt_driver_gatt_get_watchdog_timer_id(void) {
  return 0;
}
