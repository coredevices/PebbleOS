/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/endpoint.h>
#include <pbl/pp/protocol.h>
#include <pbl/pp/send.h>
#include <pbl/pp/session.h>
#include <pbl/services/firmware_update.h>
#include <pbl/services/put_bytes/put_bytes.h>

#include <kernel/events.h>
#include <kernel/language_pack_protocol.h>
#include <kernel/pebble_tasks.h>
#include <kernel/system_message.h>
#include <system/reboot_reason.h>

void system_message_send(SystemMessageType type) {
  uint8_t buffer[2] = {0x00, type};
  struct pbl_pp_session *system_session = pbl_pp_session_get_system();
  pbl_pp_send(system_session, PBL_PP_ENDPOINT_SYSTEM_MESSAGE, buffer, sizeof(buffer),
              PBL_PP_DEFAULT_TIMEOUT_MS);
  PBL_LOG_DBG("Sending sysmsg: %u", type);
}

static void prv_reset_kernel_bg_cb(void *unused) {
  PBL_LOG_ALWAYS("Rebooting to install firmware...");
  RebootReason reason = {RebootReasonCode_SoftwareUpdate, 0};
  reboot_reason_set(&reason);
  pbl_reboot();
}

static void prv_ui_update_reset_delay_timer_callback(struct pbl_work *work) {
  pbl_sys_work_call(prv_reset_kernel_bg_cb, nullptr);
}

static PBL_DELAYED_WORK_DEFINE(s_reset_delay_timer, prv_ui_update_reset_delay_timer_callback);

static void prv_handle_firmware_complete_msg(void) {
  uint32_t timeout = 3000;

  // Wait 3 seconds before rebooting so there is time to show the update complete screen
  PBL_LOG_ALWAYS("Delaying reset by 3s so the UI can update...");
  // Not rescheduled: a repeated message must not delay the reset.
  pbl_delayed_work_schedule(&pbl_sys_workq_high, &s_reset_delay_timer, PBL_MSEC(timeout));
}

//! Note: For now we just call into storage directly for the status of FW installs. Someday,
//! it would be nice for this exchange to take place as part of PutBytes
extern bool pb_storage_get_status(PutBytesObjectType obj_type, PbInstallStatus *status);
static void prv_handle_firmware_status_request(struct pbl_pp_session *session) {
  struct PBL_PACKED {
    uint8_t deprecated;
    uint8_t type;
    uint8_t rsvd[2];
    uint32_t resource_bytes_written;
    uint32_t resource_crc;
    uint32_t firmware_bytes_written;
    uint32_t firmware_crc;
  } fw_status_resp = {
    .type = SysMsgFirmwareStatusResponse,
  };

  PbInstallStatus status = {};
  if (pb_storage_get_status(ObjectFirmware, &status)) {
    fw_status_resp.firmware_bytes_written = status.num_bytes_written;
    fw_status_resp.firmware_crc = status.crc_of_bytes;
  }

  if (pb_storage_get_status(ObjectSysResources, &status)) {
    fw_status_resp.resource_bytes_written = status.num_bytes_written;
    fw_status_resp.resource_crc = status.crc_of_bytes;
  }

  PBL_LOG_DBG("FW Status Resp: res %" PRIu32 " : 0x%x fw %" PRIu32 " : 0x%x",
              fw_status_resp.resource_bytes_written, (int)fw_status_resp.resource_crc,
              fw_status_resp.firmware_bytes_written, (int)fw_status_resp.firmware_crc);

  pbl_pp_send(session, PBL_PP_ENDPOINT_SYSTEM_MESSAGE, (uint8_t *)&fw_status_resp,
              sizeof(fw_status_resp), PBL_PP_DEFAULT_TIMEOUT_MS);
}

void sys_msg_protocol_msg_callback(struct pbl_pp_session *session, const uint8_t *data,
                                   size_t length) {
  PBL_ASSERT_RUNNING_FROM_EXPECTED_TASK(PebbleTask_SysWorkq);

  SystemMessageType t = data[1];

  PBL_LOG_DBG("Received sysmsg: %u", t);

  switch (t) {
    case SysMsgFirmwareAvailable_Deprecated: {
      PBL_LOG_DBG("Deprecated available message received.");
      break;
    }

    case SysMsgFirmwareStart: {
      PBL_LOG_VERBOSE("About to receive new firmware!");

      uint32_t bytes_transferred = 0;
      uint32_t total_size = 0;

      bool smooth_progress_supported =
          pbl_pp_session_has_capability(session, PBL_PP_CAP_SMOOTH_FW_INSTALL_PROGRESS) &&
          (length >= sizeof(SysMsgSmoothFirmwareStartPayload));

      if (smooth_progress_supported) {
        SysMsgSmoothFirmwareStartPayload *payload = (SysMsgSmoothFirmwareStartPayload *)data;
        bytes_transferred = payload->bytes_already_transferred;
        total_size = bytes_transferred + payload->bytes_to_transfer;
        PBL_LOG_INFO("Starting FW update, %" PRIu32 " of %" PRIu32 " bytes already transferred",
                     bytes_transferred, total_size);
      }

      PebbleEvent e = {
        .type = PEBBLE_SYSTEM_MESSAGE_EVENT,
        .firmware_update = {
          .type = smooth_progress_supported ? PebbleSystemMessageFirmwareUpdateStart
                                            : PebbleSystemMessageFirmwareUpdateStartLegacy,
          .bytes_transferred = bytes_transferred,
          .total_transfer_size = total_size,
        }
      };
      event_put(&e);
      break;
    }

    case SysMsgFirmwareStatus:
      prv_handle_firmware_status_request(session);
      break;

    case SysMsgFirmwareComplete: {
      PBL_LOG_VERBOSE("Firmware transfer succeeded, okay to restart!");
      PebbleEvent e = {
        .type = PEBBLE_SYSTEM_MESSAGE_EVENT,
        .firmware_update.type = PebbleSystemMessageFirmwareUpdateComplete,
      };
      event_put(&e);
      prv_handle_firmware_complete_msg();
      break;
    }

    case SysMsgFirmwareFail: {
      PBL_LOG_VERBOSE("Firmware transfer failed, time to clean up!");
      PebbleEvent e = {
        .type = PEBBLE_SYSTEM_MESSAGE_EVENT,
        .firmware_update.type = PebbleSystemMessageFirmwareUpdateFailed,
      };
      event_put(&e);
      break;
    }

    case SysMsgFirmwareUpToDate: {
      PBL_LOG_VERBOSE("Firmware is up to date!");
      PebbleEvent e = {
        .type = PEBBLE_SYSTEM_MESSAGE_EVENT,
        .firmware_update.type = PebbleSystemMessageFirmwareUpToDate,
      };
      event_put(&e);
      break;
    }

#ifndef CONFIG_RECOVERY_FW
    case SysMsgLanguagePacks:
      language_pack_protocol_handle_list(session, data, length);
      break;

    case SysMsgLanguagePackRemove:
      language_pack_protocol_handle_remove(session, data, length);
      break;

    case SysMsgLanguageSelect:
      language_pack_protocol_handle_select(session, data, length);
      break;
#endif

    default:
      PBL_LOG_ERR("Invalid message received, type is %u", data[1]);
      break;
  }
}

PBL_PP_ENDPOINT_DEFINE_MAIN(PBL_PP_ENDPOINT_SYSTEM_MESSAGE, PBL_PP_ACCESS_PRIVATE,
                            sys_msg_protocol_msg_callback);

void system_message_send_firmware_start_response(FirmwareUpdateStatus status) {
  struct PBL_PACKED {
    uint8_t zero;
    uint8_t type;
    uint8_t status;
  } msg = {.zero = 0x00, .type = SysMsgFirmwareStartResponse, .status = status};

  struct pbl_pp_session *session = pbl_pp_session_get_system();
  pbl_pp_send(session, PBL_PP_ENDPOINT_SYSTEM_MESSAGE, (const uint8_t *)&msg, sizeof(msg),
              PBL_PP_DEFAULT_TIMEOUT_MS);
}
