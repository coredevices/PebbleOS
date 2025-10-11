/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

/*
 * firmware_metadata.h
 *
 * This file specifies the Firmware Metadata structure used in the .elf & .bin files to
 * identify the build info, etc.
 */

#include "util/attributes.h"

#include <stdint.h>
#include <stdbool.h>


#define FW_METADATA_CURRENT_STRUCT_VERSION 0x1
#define FW_METADATA_VERSION_SHORT_BYTES 8
#define FW_METADATA_VERSION_TAG_BYTES 32

// NOTE: When adding new platforms, if they use the legacy defective CRC, the list in
// tools/fw_binary_info.py needs to be updated with the platform value.
typedef enum FirmwareMetadataPlatform {
  FirmwareMetadataPlatformUnknown = 0,
  FirmwareMetadataPlatformPebbleOneEV1 = 1,
  FirmwareMetadataPlatformPebbleOneEV2 = 2,
  FirmwareMetadataPlatformPebbleOneEV2_3 = 3,
  FirmwareMetadataPlatformPebbleOneEV2_4 = 4,
  FirmwareMetadataPlatformPebbleOnePointFive = 5,
  FirmwareMetadataPlatformPebbleTwoPointZero = 6,
  FirmwareMetadataPlatformPebbleSnowyEVT2 = 7,
  FirmwareMetadataPlatformPebbleSnowyDVT = 8,
  FirmwareMetadataPlatformPebbleSpaldingEVT = 9,
  FirmwareMetadataPlatformPebbleBobbyDVT = 10,
  FirmwareMetadataPlatformPebbleSpalding = 11,
  FirmwareMetadataPlatformPebbleSilkEVT = 12,
  FirmwareMetadataPlatformPebbleRobertEVT = 13,
  FirmwareMetadataPlatformPebbleSilk = 14,
  FirmwareMetadataPlatformPebbleAsterix = 15,
  FirmwareMetadataPlatformPebbleObelix = 16,

  FirmwareMetadataPlatformPebbleOneBigboard = 0xff,
  FirmwareMetadataPlatformPebbleOneBigboard2 = 0xfe,
  FirmwareMetadataPlatformPebbleSnowyBigboard = 0xfd,
  FirmwareMetadataPlatformPebbleSnowyBigboard2 = 0xfc,
  FirmwareMetadataPlatformPebbleSpaldingBigboard = 0xfb,
  FirmwareMetadataPlatformPebbleSilkBigboard = 0xfa,
  FirmwareMetadataPlatformPebbleRobertBigboard = 0xf9,
  FirmwareMetadataPlatformPebbleSilkBigboard2 = 0xf8,
  FirmwareMetadataPlatformPebbleRobertBigboard2 = 0xf7,
} FirmwareMetadataPlatform;

// WARNING: changes in this struct must be reflected in:
// - iOS/PebblePrivateKit/PebblePrivateKit/PBBundle.m

struct PACKED FirmwareMetadata {
  uint32_t version_timestamp;
  char version_tag[FW_METADATA_VERSION_TAG_BYTES];
  char version_short[FW_METADATA_VERSION_SHORT_BYTES];
  bool is_recovery_firmware:1;
  bool is_ble_firmware:1;
  bool is_dual_slot:1;
  bool is_slot_0:1;
  uint8_t reserved:4;
  uint8_t hw_platform;
  //! This should be the last field, since we put the meta data struct at the end of the fw binary.
  uint8_t metadata_version;
};
typedef struct FirmwareMetadata FirmwareMetadata;

_Static_assert(sizeof(struct FirmwareMetadata) == (sizeof(uint32_t) +
               FW_METADATA_VERSION_SHORT_BYTES + FW_METADATA_VERSION_TAG_BYTES + sizeof(uint8_t) +
               sizeof(uint8_t) + sizeof(uint8_t)),
               "FirmwareMetadata bitfields not packed correctly");


// Shared defines. Let's not duplicate this everywhere.

#ifdef RECOVERY_FW
  #define FIRMWARE_METADATA_IS_RECOVERY_FIRMWARE (true)
#else
  #define FIRMWARE_METADATA_IS_RECOVERY_FIRMWARE (false)
#endif

#if BOARD_BIGBOARD
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleOneBigboard)
#elif BOARD_BB2
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleOneBigboard2)
#elif BOARD_SNOWY_BB
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleSnowyBigboard)
#elif BOARD_SNOWY_BB2
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleSnowyBigboard2)
#elif BOARD_SNOWY_EVT2
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleSnowyEVT2)
#elif BOARD_SNOWY_DVT
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleSnowyDVT)
#elif BOARD_SNOWY_S3
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleBobbyDVT)
#elif BOARD_V2_0
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleTwoPointZero)
#elif BOARD_V1_5
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleOnePointFive)
#elif BOARD_EV2_4
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleOneEV2_4)
#elif BOARD_SPALDING_BB2
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleSpaldingBigboard)
#elif BOARD_SPALDING_EVT
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleSpaldingEVT)
#elif BOARD_SPALDING
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleSpalding)
#elif BOARD_SILK_EVT
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleSilkEVT)
#elif BOARD_SILK_BB
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleSilkBigboard)
#elif BOARD_SILK
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleSilk)
#elif BOARD_SILK_BB2
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleSilkBigboard2)
#elif BOARD_ROBERT_BB
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleRobertBigboard)
#elif BOARD_ROBERT_BB2
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleRobertBigboard2)
#elif BOARD_ROBERT_EVT
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleRobertEVT)
#elif BOARD_ASTERIX
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleAsterix)
#elif BOARD_OBELIX || BOARD_OBELIX_BB || BOARD_OBELIX_BB2
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformPebbleObelix)
#else
  #define FIRMWARE_METADATA_HW_PLATFORM (FirmwareMetadataPlatformUnknown)
#endif
