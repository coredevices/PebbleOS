/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel/events.h"
#include "services/normal/accessory/smartstrap_attribute.h"
#include "services/normal/accessory/smartstrap_comms.h"
#include "services/normal/accessory/smartstrap_link_control.h"
#include "services/normal/accessory/smartstrap_state.h"
#include "system/logging.h"
#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"
#include "util/mbuf.h"

#define RAW_DATA_MAX_SERVICES 1


static bool prv_read_complete(bool success, uint32_t length) {
  // we don't allow reads of more than UINT16_MAX
  if (length > UINT16_MAX) {
    PBL_LOG(LOG_LEVEL_WARNING,
            "Got read of length %"PRIu32" which is longer than UINT16_MAX", length);
    success = false;
  }
  // send the read complete event directly to the app
  SmartstrapResult result = success ? SmartstrapResultOk : SmartstrapResultTimeOut;
  smartstrap_attribute_send_event(SmartstrapDataReceivedEvent, SmartstrapProfileRawData, result,
                                  SMARTSTRAP_RAW_DATA_SERVICE_ID, SMARTSTRAP_RAW_DATA_ATTRIBUTE_ID,
                                  length);
  return success;
}

static void prv_handle_notification(void) {
  smartstrap_attribute_send_event(SmartstrapNotifyEvent, SmartstrapProfileRawData,
                                  SmartstrapResultOk, SMARTSTRAP_RAW_DATA_SERVICE_ID,
                                  SMARTSTRAP_RAW_DATA_ATTRIBUTE_ID, 0);
}

static void prv_set_connected(bool connected) {
  if (connected && smartstrap_link_control_is_profile_supported(SmartstrapProfileRawData)) {
    smartstrap_connection_state_set_by_service(SMARTSTRAP_RAW_DATA_SERVICE_ID, true);
  }
}

static SmartstrapResult prv_send(const SmartstrapRequest *request) {
  return smartstrap_send(SmartstrapProfileRawData, request->write_mbuf, request->read_mbuf,
                         request->timeout_ms);
}


const SmartstrapProfileInfo *smartstrap_raw_data_get_info(void) {
  static const SmartstrapProfileInfo s_profile_info = {
    .profile = SmartstrapProfileRawData,
    .max_services = RAW_DATA_MAX_SERVICES,
    .min_service_id = SMARTSTRAP_RAW_DATA_SERVICE_ID,
    .connected = prv_set_connected,
    .send = prv_send,
    .read_complete = prv_read_complete,
    .notify = prv_handle_notification,
  };
  return &s_profile_info;
}
