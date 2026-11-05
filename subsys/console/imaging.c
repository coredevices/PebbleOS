/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "console_private.h"

#include <stdint.h>
#include <string.h>

#include <pbl/drivers/flash.h>
#include <pbl/kernel/compiler.h>

#include <flash_region/flash_region.h>
#include <resource/resource_storage_flash.h>
#include <system/bootbits.h>

#define CMD_ERASE           1U
#define CMD_WRITE           2U
#define CMD_CRC             3U
#define CMD_QUERY_REGION    4U
#define CMD_FINALIZE_REGION 5U

#define RESP_ERASE_DONE 128U
#define RESP_CRC        130U
#define RESP_REGION     131U
#define RESP_FINALIZED  132U
#define RESP_ERROR      193U

#define REGION_RECOVERY  1U
#define REGION_RESOURCES 2U

typedef struct PBL_PACKED {
  uint32_t address;
  uint32_t length;
} ImagingRange;

typedef struct PBL_PACKED {
  uint8_t opcode;
  uint32_t address;
  uint32_t length;
} ImagingRangeResponse;

typedef struct PBL_PACKED {
  uint8_t opcode;
  uint32_t crc;
} ImagingCrcResponse;

static void prv_send(const void *data, size_t len) {
  pbl_console__frame_send(CONSOLE_FRAME_CHANNEL_IMAGING, 0U, data, len);
}

static void prv_send_opcode(uint8_t opcode) {
  prv_send(&opcode, sizeof(opcode));
}

static void prv_erase_complete(int status, void *arg) {
  prv_send_opcode((status == 0) ? RESP_ERASE_DONE : RESP_ERROR);
}

static void prv_handle_erase(const uint8_t *args, size_t len) {
  ImagingRange range;

  if (len != sizeof(range)) {
    prv_send_opcode(RESP_ERROR);
    return;
  }
  memcpy(&range, args, sizeof(range));

  if (pbl_flash_erase_async(FLASH, range.address, range.length, prv_erase_complete, nullptr) != 0) {
    prv_send_opcode(RESP_ERROR);
  }
}

static void prv_handle_write(const uint8_t *args, size_t len) {
  uint32_t address;

  if (len <= sizeof(address)) {
    prv_send_opcode(RESP_ERROR);
    return;
  }
  memcpy(&address, args, sizeof(address));

  // The host learns of success from the frame's acknowledgment, which follows this.
  if (pbl_flash_write(FLASH, address, &args[sizeof(address)], len - sizeof(address)) != 0) {
    prv_send_opcode(RESP_ERROR);
  }
}

static void prv_handle_crc(const uint8_t *args, size_t len) {
  ImagingRange range;

  if (len != sizeof(range)) {
    prv_send_opcode(RESP_ERROR);
    return;
  }
  memcpy(&range, args, sizeof(range));

  const ImagingCrcResponse resp = {
    .opcode = RESP_CRC,
    .crc = pbl_flash_crc32(FLASH, range.address, range.length),
  };
  prv_send(&resp, sizeof(resp));
}

static void prv_handle_query_region(uint8_t region) {
  ImagingRangeResponse resp = {.opcode = RESP_REGION};

  switch (region) {
    case REGION_RECOVERY:
      // A query means the region is about to be written.
      (void)pbl_flash_unprotect(FLASH);
      resp.address = FLASH_REGION_SAFE_FIRMWARE_BEGIN;
      resp.length = FLASH_REGION_SAFE_FIRMWARE_END - FLASH_REGION_SAFE_FIRMWARE_BEGIN;
      break;
    case REGION_RESOURCES: {
      const SystemResourceBank *bank = resource_storage_flash_get_unused_bank();
      resp.address = bank->begin;
      resp.length = bank->end - bank->begin;
      break;
    }
    default:
      prv_send_opcode(RESP_ERROR);
      return;
  }

  prv_send(&resp, sizeof(resp));
}

static void prv_handle_finalize_region(uint8_t region) {
  switch (region) {
    case REGION_RECOVERY:
      (void)pbl_flash_protect(FLASH, FLASH_REGION_SAFE_FIRMWARE_BEGIN,
                              FLASH_REGION_SAFE_FIRMWARE_END - FLASH_REGION_SAFE_FIRMWARE_BEGIN);
      break;
    case REGION_RESOURCES:
      boot_bit_set(BOOT_BIT_NEW_SYSTEM_RESOURCES_AVAILABLE);
      break;
    default:
      prv_send_opcode(RESP_ERROR);
      return;
  }

  prv_send_opcode(RESP_FINALIZED);
}

void pbl_console__imaging_handle_frame(const void *data, size_t len) {
  const uint8_t *bytes = data;

  if (len < 1U) {
    prv_send_opcode(RESP_ERROR);
    return;
  }

  const uint8_t *args = &bytes[1];
  const size_t args_len = len - 1U;

  switch (bytes[0]) {
    case CMD_ERASE:
      prv_handle_erase(args, args_len);
      break;
    case CMD_WRITE:
      prv_handle_write(args, args_len);
      break;
    case CMD_CRC:
      prv_handle_crc(args, args_len);
      break;
    case CMD_QUERY_REGION:
      if (args_len == 1U) {
        prv_handle_query_region(args[0]);
      } else {
        prv_send_opcode(RESP_ERROR);
      }
      break;
    case CMD_FINALIZE_REGION:
      if (args_len == 1U) {
        prv_handle_finalize_region(args[0]);
      } else {
        prv_send_opcode(RESP_ERROR);
      }
      break;
    default:
      prv_send_opcode(RESP_ERROR);
      break;
  }
}
