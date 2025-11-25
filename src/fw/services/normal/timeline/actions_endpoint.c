/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "actions_endpoint.h"
#include "attributes_actions.h"

#include "comm/ble/kernel_le_client/ancs/ancs_types.h"
#include "kernel/pbl_malloc.h"
#include "services/common/system_task.h"
#include "services/normal/notifications/notification_storage.h"
#include "services/normal/notifications/notifications.h"
#include "services/normal/timeline/timeline_actions.h"
#include "system/hexdump.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/attributes.h"

typedef enum {
  CommandInvokeAction = 0x02,
  CommandInvokeActionANCSNotif = 0x03,
  CommandPhoneResponse = 0x11,
  CommandPhoneActionResponse = 0x12,
} Command;

typedef enum {
  ResponseACK = 0x00,
  ResponseNACK = 0x01,
  ResponseACKANCSDismiss = 0x0F,
  ResponseNACKContactAmbiguity = 0x10,
  ResponseNACKContactNotFound = 0x11,
  ResponseNACKAddressAmbiguity = 0x12,
  ResponseNACKAddressNotFound = 0x13,
  ResponseNACKGroupSMSNotSupported = 0x14,
  ResponseNACKStartReply = 0x15,
} Response;

typedef struct PACKED {
  Command command:8;
  Uuid item_id;
  Response response:8;
} ResponseHeader;

typedef struct PACKED {
  ResponseHeader header;
  uint8_t num_attributes;
  uint8_t data[];
} PhoneResponseMsg;

typedef struct PACKED {
  ResponseHeader header;
  uint8_t num_attributes;
  uint8_t num_actions;
  uint8_t data[];
} PhoneActionResponseMsg;

typedef struct PACKED {
  Command command:8;
  Uuid item_id;
  uint8_t action_id;
  uint8_t num_attributes;
  uint8_t data[];
} InvokeActionMsg;

typedef struct {
  size_t length;
  InvokeActionMsg msg;
} InvokeActionMsgCbData;

T_STATIC const int TIMELINE_ACTION_ENDPOINT = 0x2cb0;


static void prv_action_system_task_callback(void *data) {
  InvokeActionMsgCbData *action = data;

  comm_session_send_data(comm_session_get_system_session(), TIMELINE_ACTION_ENDPOINT,
      (uint8_t *)&action->msg, action->length, COMM_SESSION_DEFAULT_TIMEOUT);

  kernel_free(action);
}


static ActionResultType prv_get_action_result_type(ResponseHeader *header) {
  switch (header->response) {
    case ResponseACK:
      return ActionResultTypeSuccess;
    case ResponseNACKContactAmbiguity:
    case ResponseNACKAddressAmbiguity:
      return ActionResultTypeChaining;
    case ResponseNACKStartReply:
      return ActionResultTypeDoResponse;
    case ResponseACKANCSDismiss:
      return ActionResultTypeSuccessANCSDismiss;
    default:
      return ActionResultTypeFailure;
  }
}

static PebbleSysNotificationActionResult *prv_action_result_create_from_serial_data(
    ResponseHeader *header, uint8_t num_attributes, uint8_t num_actions,
    const uint8_t *data, size_t size) {

  size_t string_alloc_size;
  uint8_t attributes_per_action[num_actions];
  bool r = attributes_actions_parse_serial_data(num_attributes, num_actions, data, size,
                                                &string_alloc_size, attributes_per_action);
  if (!r) {
    return NULL;
  }

  const size_t alloc_size = attributes_actions_get_required_buffer_size(num_attributes, num_actions,
                                                                        attributes_per_action,
                                                                        string_alloc_size);

  PebbleSysNotificationActionResult *action_result =
      kernel_zalloc(sizeof(PebbleSysNotificationActionResult) + alloc_size);
  if (!action_result) {
    PBL_LOG(LOG_LEVEL_WARNING, "Failed to allocate memory for action result");
    return NULL;
  }

  uint8_t *buffer = (uint8_t *)action_result + sizeof(PebbleSysNotificationActionResult);
  uint8_t *const buf_end = buffer + alloc_size;

  action_result->id = header->item_id;
  action_result->type = prv_get_action_result_type(header);

  attributes_actions_init(&action_result->attr_list, &action_result->action_group,
                          &buffer, num_attributes, num_actions, attributes_per_action);

  if (!attributes_actions_deserialize(&action_result->attr_list, &action_result->action_group,
                                      buffer, buf_end, data, size)) {
    goto cleanup;
  }

  return action_result;

cleanup:
  kernel_free(action_result);
  return NULL;
}

void timeline_action_endpoint_invoke_action(const Uuid *id, TimelineItemActionType type,
                                            uint8_t action_id, const AttributeList *attributes,
                                            bool do_async) {
  size_t attr_data_size = (attributes != NULL) ? attribute_list_get_serialized_size(attributes) : 0;
  InvokeActionMsgCbData *invoke_action_data =
      kernel_zalloc_check(sizeof(InvokeActionMsgCbData) + attr_data_size);

  invoke_action_data->length = sizeof(invoke_action_data->msg) + attr_data_size;
  if (type == TimelineItemActionTypeAncsResponse || type == TimelineItemActionTypeAncsGeneric) {
    invoke_action_data->msg.command = CommandInvokeActionANCSNotif;
  } else {
    invoke_action_data->msg.command = CommandInvokeAction;
  }
  invoke_action_data->msg.action_id = action_id;
  invoke_action_data->msg.item_id = *id;
  if (attributes != NULL) {
    invoke_action_data->msg.num_attributes = attributes->num_attributes;
    size_t added_data_size = attribute_list_serialize(attributes, invoke_action_data->msg.data,
        invoke_action_data->msg.data + attr_data_size);
    PBL_ASSERTN(added_data_size == attr_data_size);
  } else {
    invoke_action_data->msg.num_attributes = 0;
  }

  char uuid_string[UUID_STRING_BUFFER_LENGTH];
  uuid_to_string(id, uuid_string);
  PBL_LOG(LOG_LEVEL_INFO, "Send action to phone (Item ID: %s; Action ID: %d)",
      uuid_string, action_id);

  PBL_HEXDUMP(LOG_LEVEL_DEBUG, (uint8_t *)&invoke_action_data->msg, invoke_action_data->length);

  if (do_async) {
    system_task_add_callback(prv_action_system_task_callback, invoke_action_data);
  } else {
    comm_session_send_data(comm_session_get_system_session(), TIMELINE_ACTION_ENDPOINT,
        (uint8_t *)&invoke_action_data->msg, invoke_action_data->length,
        COMM_SESSION_DEFAULT_TIMEOUT);
    kernel_free(invoke_action_data);
  }
}

void timeline_action_endpoint_protocol_msg_callback(CommSession *session,
    const uint8_t* data, size_t length) {
  if (length < sizeof(ResponseHeader)) {
    PBL_LOG(LOG_LEVEL_WARNING, "Invalid phone response message length %d", (int) length);
    return;
  }

  ResponseHeader *header = (ResponseHeader *) data;
  if (header->command != CommandPhoneResponse && header->command != CommandPhoneActionResponse) {
    PBL_LOG(LOG_LEVEL_WARNING, "Invalid command id");
    return;
  }

  if (uuid_is_system(&header->item_id)) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Automatic SMS msg response: 0x%02X", header->response);
    return;
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "Action Endpoint Response: 0x%02X", header->response);

  PebbleSysNotificationActionResult *action_result = NULL;

  PBL_HEXDUMP(LOG_LEVEL_DEBUG, data, length);

  if (header->command == CommandPhoneResponse) {
    PhoneResponseMsg *msg = (PhoneResponseMsg *)data;
    action_result = prv_action_result_create_from_serial_data(
        header, msg->num_attributes, 0, msg->data, length - sizeof(PhoneResponseMsg));
  } else {
    PhoneActionResponseMsg *msg = (PhoneActionResponseMsg *)data;
    action_result = prv_action_result_create_from_serial_data(
        header, msg->num_attributes, msg->num_actions, msg->data,
        (length - sizeof(PhoneActionResponseMsg)));
  }

  if (action_result) {
    // callee will free memory
    notifications_handle_notification_action_result(action_result);
  }
}
