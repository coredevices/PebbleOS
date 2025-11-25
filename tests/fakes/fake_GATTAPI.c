/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_GATTAPI.h"

#include "clar_asserts.h"

#include <string.h>

static GATT_Connection_Event_Callback_t s_connection_event_callback;
static unsigned int s_stack_id;
static unsigned long s_connection_callback_param;

static int s_start_count;
static int s_stop_count;

static int s_start_ret_val;
static int s_stop_ret_val;

static int s_service_changed_indication_count;

struct FakeGATTServiceDiscoveryContext {
  bool is_running;
  unsigned int stack_id;
  unsigned int connection_id;
  unsigned int num_of_uuids;
  GATT_UUID_t *uuids;
  GATT_Service_Discovery_Event_Callback_t callback;
  unsigned long callback_param;
} s_service_discovery_ctx;

int GATT_Initialize(unsigned int BluetoothStackID,
                    unsigned long Flags,
                    GATT_Connection_Event_Callback_t ConnectionEventCallback,
                    unsigned long CallbackParameter) {
  s_stack_id = BluetoothStackID;
  s_connection_event_callback = ConnectionEventCallback;
  s_connection_callback_param = CallbackParameter;
  return 0;
}

int GATT_Cleanup(unsigned int BluetoothStackID) {
  return 0;
}

int GATT_Start_Service_Discovery_Handle_Range(unsigned int stack_id,
                                 unsigned int connection_id,
                                 GATT_Attribute_Handle_Group_t *DiscoveryHandleRange,
                                 unsigned int NumberOfUUID,
                                 GATT_UUID_t *UUIDList,
                                 GATT_Service_Discovery_Event_Callback_t ServiceDiscoveryCallback,
                                 unsigned long CallbackParameter) {
  s_service_discovery_ctx = (struct FakeGATTServiceDiscoveryContext) {
    .is_running = true,
    .stack_id = stack_id,
    .connection_id = connection_id,
    .num_of_uuids = NumberOfUUID,
    .uuids = UUIDList,
    .callback = ServiceDiscoveryCallback,
    .callback_param = CallbackParameter
  };
  ++s_start_count;
  return s_start_ret_val;
}

int GATT_Stop_Service_Discovery(unsigned int BluetoothStackID, unsigned int ConnectionID) {
  s_service_discovery_ctx.is_running = false;
  ++s_stop_count;
  return s_stop_ret_val;
}

bool fake_gatt_is_service_discovery_running(void) {
  return s_service_discovery_ctx.is_running;
}

int fake_gatt_is_service_discovery_start_count(void) {
  return s_start_count;
}

int fake_gatt_is_service_discovery_stop_count(void) {
  return s_stop_count;
}

void fake_gatt_set_start_return_value(int ret_value) {
  s_start_ret_val = ret_value;
}

void fake_gatt_set_stop_return_value(int ret_value) {
  s_stop_ret_val = ret_value;
}

void fake_gatt_put_service_discovery_event(GATT_Service_Discovery_Event_Data_t *event) {
  cl_assert_equal_b(s_service_discovery_ctx.is_running, true);
  if (event->Event_Data_Type == etGATT_Service_Discovery_Complete) {
    s_service_discovery_ctx.is_running = false;
  }
  s_service_discovery_ctx.callback(s_service_discovery_ctx.stack_id,
                                   event,
                                   s_service_discovery_ctx.callback_param);
}

void fake_gatt_init(void) {
  memset(&s_service_discovery_ctx, 0,
         sizeof(struct FakeGATTServiceDiscoveryContext));
  s_stack_id = 0;
  s_connection_callback_param = 0;
  s_connection_event_callback = NULL;
  s_start_count = 0;
  s_stop_count = 0;
  s_start_ret_val = 0;
  s_stop_ret_val = 0;
  s_service_changed_indication_count = 0;
}

int GATT_Service_Changed_CCCD_Read_Response(unsigned int BluetoothStackID,
                                            unsigned int TransactionID,
                                            Word_t CCCD) {
  return 0;
}

int GATT_Service_Changed_Indication(unsigned int BluetoothStackID,
                                    unsigned int ConnectionID,
                                    GATT_Service_Changed_Data_t *Service_Changed_Data) {
  ++s_service_changed_indication_count;
  return 1; // fake transaction ID
}

int fake_gatt_get_service_changed_indication_count(void) {
  return s_service_changed_indication_count;
}

int GATT_Service_Changed_Read_Response(unsigned int BluetoothStackID,
                                       unsigned int TransactionID,
                                       GATT_Service_Changed_Data_t *Service_Changed_Data) {
  return 0;
}

static uint16_t s_write_request_length;
static GATT_Client_Event_Callback_t s_write_cb;
static unsigned long s_write_cb_param;
static unsigned int s_write_connection_id;
static unsigned int s_write_stack_id;
static uint16_t s_write_handle;

int GATT_Write_Request(unsigned int BluetoothStackID, unsigned int ConnectionID,
                       Word_t AttributeHandle, Word_t AttributeLength, void *AttributeValue,
                       GATT_Client_Event_Callback_t ClientEventCallback,
                       unsigned long CallbackParameter) {
  s_write_handle = AttributeHandle;
  s_write_request_length = AttributeLength;
  s_write_cb = ClientEventCallback;
  s_write_cb_param = CallbackParameter;
  s_write_connection_id = ConnectionID;
  s_write_stack_id = BluetoothStackID;
  return 1;
}

uint16_t fake_gatt_write_last_written_handle(void) {
  return s_write_handle;
}

void fake_gatt_put_write_response_for_last_write(void) {
  cl_assert_(s_write_cb, "GATT_Write_Request need to be called first!");
  GATT_Write_Response_Data_t data = {
    .ConnectionID = s_write_connection_id,
    .TransactionID = 1,
    .ConnectionType = gctLE,
//    .RemoteDevice // TODO
    .BytesWritten = s_write_request_length,
  };
  GATT_Client_Event_Data_t event = {
    .Event_Data_Type = etGATT_Client_Write_Response,
    .Event_Data_Size = sizeof(GATT_Write_Response_Data_t),
    .Event_Data.GATT_Write_Response_Data = &data,
  };
  s_write_cb(s_write_stack_id, &event, s_write_cb_param);
  s_write_cb = NULL;
}
