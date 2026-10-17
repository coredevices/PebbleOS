/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/services/firmware_update.h>

#define SYS_MSG_ENDPOINT_ID 0x12

typedef enum SystemMessageType {
  SysMsgFirmwareAvailable_Deprecated = 0x00,
  SysMsgFirmwareStart = 0x01,
  SysMsgFirmwareComplete = 0x02,
  SysMsgFirmwareFail = 0x03,
  SysMsgFirmwareUpToDate = 0x04,
  // SysMsgFirmWareOutOfDate = 0x05, DEPRECATED
  SysMsgReconnectRequestStop = 0x06,
  SysMsgReconnectRequestStart = 0x07,
  SysMsgMAPRetry = 0x08,     // MAP is no longer used
  SysMsgMAPConnected = 0x09, // MAP is no longer used
  SysMsgFirmwareStartResponse = 0x0a,
  SysMsgFirmwareStatus = 0x0b,         // Phone -> Watch request for partial fw install info
  SysMsgFirmwareStatusResponse = 0x0c, // Watch -> Phone response of what fw is partially installed
  SysMsgLanguagePacks = 0x0d,
  SysMsgLanguagePacksResponse = 0x0e,
  SysMsgLanguagePackRemove = 0x0f,
  SysMsgLanguagePackRemoveResponse = 0x10,
  SysMsgLanguageSelect = 0x11,
  // 0x12 is skipped to avoid confusion with the endpoint ID
  SysMsgLanguageSelectResponse = 0x13,
} SystemMessageType;

typedef struct PBL_PACKED SysMsgSmoothFirmwareStartPayload {
  uint8_t deprecated;         // not used anymore but all messages start with 0x0
  SystemMessageType type : 8; // == SysMsgFirmwareStart
  // The number of bytes the phone has transferred in a previous operation
  uint32_t bytes_already_transferred;
  // The total number of bytes the phone needs to transfer to complete the firmware update
  // (i.e For a normal firmware, this would be the sum of outstanding bytes for the fw binary
  // and also the pbpack)
  uint32_t bytes_to_transfer;
} SysMsgSmoothFirmwareStartPayload;

void system_message_init(void);

void system_message_send(SystemMessageType type);

void system_message_send_firmware_start_response(FirmwareUpdateStatus status);
