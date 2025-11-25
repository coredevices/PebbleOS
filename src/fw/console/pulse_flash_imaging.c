/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "console/pulse_protocol_impl.h"

#include <stdint.h>
#include <string.h>

#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "kernel/util/sleep.h"
#include "resource/resource_storage_flash.h"
#include "services/common/system_task.h"
#include "system/bootbits.h"
#include "util/attributes.h"
#include "util/math.h"

#define IMAGING_CMD_ERASE (1)
#define IMAGING_CMD_WRITE (2)
#define IMAGING_CMD_CRC (3)
#define IMAGING_CMD_QUERY_REGION (4)
#define IMAGING_CMD_FINALIZE_REGION (5)

#define IMAGING_RESP_ACK_ERASE (128)
#define IMAGING_RESP_ACK_WRITE (129)
#define IMAGING_RESP_CRC (130)
#define IMAGING_RESP_REGION_GEOMETRY (131)
#define IMAGING_RESP_FINALIZE_REGION (132)

#define IMAGING_RESP_MALFORMED_CMD (192)
#define IMAGING_RESP_INTERNAL_ERROR (193)

#define FLASH_REGION_PRF (1)
#define FLASH_REGION_SYSTEM_RESOURCES (2)

typedef union Command {
  uint8_t opcode;
  struct PACKED EraseCommand {
    uint8_t opcode;
    uint32_t address;
    uint32_t length;
  } erase;
  struct PACKED WriteCommand {
    uint8_t opcode;
    uint32_t address;
    uint8_t data[0];
  } write;
  struct PACKED CrcCommand {
    uint8_t opcode;
    uint32_t address;
    uint32_t length;
  } crc;
  struct PACKED RegionCommand {
    uint8_t opcode;
    uint8_t region;
  } region;
} Command;


static void prv_handle_erase(Command *cmd, size_t length);
static void prv_handle_write(Command *cmd, size_t length);
static void prv_handle_crc(Command *cmd, size_t length);
static void prv_handle_query_region(Command *cmd, size_t length);
static void prv_handle_finalize_region(Command *cmd, size_t length);
static void prv_respond_malformed_command(Command *cmd, size_t length,
                                          const char *message);

void pulse_flash_imaging_handler(void *packet, size_t length) {
  Command *command = packet;
  if (length) {
    switch (command->opcode) {
      case IMAGING_CMD_ERASE:
        prv_handle_erase(command, length);
        return;
      case IMAGING_CMD_WRITE:
        prv_handle_write(command, length);
        return;
      case IMAGING_CMD_CRC:
        prv_handle_crc(command, length);
        return;
      case IMAGING_CMD_QUERY_REGION:
        prv_handle_query_region(command, length);
        return;
      case IMAGING_CMD_FINALIZE_REGION:
        prv_handle_finalize_region(command, length);
        return;
      default:
        prv_respond_malformed_command(command, length, "Unknown command opcode");
        return;
    }
  }
  prv_respond_malformed_command(command, length, "Empty command");
}

void pulse_flash_imaging_link_state_handler(PulseLinkState link_state) {
}

static bool s_erase_in_progress = false;
static uint32_t s_erase_start_address;
static uint32_t s_erase_length;

typedef struct PACKED EraseAndWriteAck {
  uint8_t opcode;
  uint32_t address;
  uint32_t length;
  uint8_t complete;
} EraseAndWriteAck;

static void prv_erase_complete(void *ignored, status_t result);

static void prv_handle_erase(Command *cmd, size_t length) {
  if (length != sizeof(cmd->erase)) {
    prv_respond_malformed_command(cmd, length, NULL);
    return;
  }

  if (s_erase_in_progress) {
    EraseAndWriteAck *ack = pulse_best_effort_send_begin(
        PULSE_PROTOCOL_FLASH_IMAGING);
    *ack = (EraseAndWriteAck) {
      .opcode = IMAGING_RESP_ACK_ERASE,
      .address = s_erase_start_address,
      .length = s_erase_length,
      .complete = 0
    };
    pulse_best_effort_send(ack, sizeof(EraseAndWriteAck));
  } else {
    s_erase_in_progress = true;
    s_erase_start_address = cmd->erase.address;
    s_erase_length = cmd->erase.length;

    EraseAndWriteAck *message = pulse_best_effort_send_begin(
        PULSE_PROTOCOL_FLASH_IMAGING);
    *message = (EraseAndWriteAck) {
      .opcode = IMAGING_RESP_ACK_ERASE,
      .address = s_erase_start_address,
      .length = s_erase_length,
      .complete = 0
    };
    pulse_best_effort_send(message, sizeof(EraseAndWriteAck));

    uint32_t end_address = cmd->erase.address + cmd->erase.length;
    flash_erase_optimal_range(
        cmd->erase.address, cmd->erase.address, end_address,
        (end_address + SECTOR_SIZE_BYTES - 1) & SECTOR_ADDR_MASK,
        prv_erase_complete, NULL);
  }
}

static void prv_erase_complete(void *ignored, status_t result) {
  EraseAndWriteAck *message = pulse_best_effort_send_begin(
      PULSE_PROTOCOL_FLASH_IMAGING);
  *message = (EraseAndWriteAck) {
    .opcode = IMAGING_RESP_ACK_ERASE,
    .address = s_erase_start_address,
    .length = s_erase_length,
    .complete = 1
  };

  if (FAILED(result)) {
    message->opcode = IMAGING_RESP_INTERNAL_ERROR;
    pulse_best_effort_send(message, sizeof(message->opcode));
  } else {
    pulse_best_effort_send(message, sizeof(EraseAndWriteAck));
  }

  s_erase_in_progress = false;
}

static void prv_handle_write(Command *cmd, size_t command_length) {
  if (command_length <= sizeof(cmd->write)) {
    prv_respond_malformed_command(cmd, command_length, NULL);
    return;
  }

  size_t write_length = command_length - sizeof(cmd->write);
  flash_write_bytes(&cmd->write.data[0], cmd->write.address, write_length);

  EraseAndWriteAck *ack = pulse_best_effort_send_begin(
      PULSE_PROTOCOL_FLASH_IMAGING);
  *ack = (EraseAndWriteAck) {
    .opcode = IMAGING_RESP_ACK_WRITE,
    .address = cmd->write.address,
    .length = write_length,
    .complete = 1
  };
  pulse_best_effort_send(ack, sizeof(EraseAndWriteAck));

  // Since packets arrive so rapidly when writing, flash imaging can consume
  // all of the available CPU time and completely block lower-priority tasks.
  // To prevent DoSing KernelBG and tripping the watchdog, suspend the current
  // task for a couple ticks after each write to let other tasks catch up.
  psleep(2);
}

static void prv_handle_crc(Command *cmd, size_t length) {
  if (length != sizeof(cmd->crc)) {
    prv_respond_malformed_command(cmd, length, NULL);
    return;
  }

  typedef struct PACKED CrcAck {
    uint8_t opcode;
    uint32_t address;
    uint32_t length;
    uint32_t crc;
  } CrcAck;

  uint32_t crc = flash_calculate_legacy_defective_checksum(cmd->crc.address, cmd->crc.length);

  CrcAck *ack = pulse_best_effort_send_begin(PULSE_PROTOCOL_FLASH_IMAGING);
  *ack = (CrcAck) {
    .opcode = IMAGING_RESP_CRC,
    .address = cmd->crc.address,
    .length = cmd->crc.length,
    .crc = crc
  };
  pulse_best_effort_send(ack, sizeof(CrcAck));
}

static void prv_handle_query_region(Command *cmd, size_t length) {
  if (length != sizeof(cmd->region)) {
    prv_respond_malformed_command(cmd, length, NULL);
    return;
  }

  uint32_t region_base, region_length;
  switch (cmd->region.region) {
    case FLASH_REGION_PRF:
      // assume a query of the region means we are going to write to it
      flash_prf_set_protection(false);
      region_base = FLASH_REGION_SAFE_FIRMWARE_BEGIN;
      region_length = FLASH_REGION_SAFE_FIRMWARE_END -
          FLASH_REGION_SAFE_FIRMWARE_BEGIN;
      break;
    case FLASH_REGION_SYSTEM_RESOURCES: {
      const SystemResourceBank *bank = resource_storage_flash_get_unused_bank();
      region_base = bank->begin;
      region_length = bank->end - bank->begin;
      break;
    }
    default:
      region_base = 0;
      region_length = 0;
      break;
  }

  typedef struct PACKED RegionGeometry {
    uint8_t opcode;
    uint8_t region;
    uint32_t address;
    uint32_t length;
  } RegionGeometry;

  RegionGeometry *resp = pulse_best_effort_send_begin(
      PULSE_PROTOCOL_FLASH_IMAGING);
  *resp = (RegionGeometry) {
    .opcode = IMAGING_RESP_REGION_GEOMETRY,
    .region = cmd->region.region,
    .address = region_base,
    .length = region_length
  };

  pulse_best_effort_send(resp, sizeof(RegionGeometry));
}

static void prv_handle_finalize_region(Command *cmd, size_t length) {
  if (length != sizeof(cmd->region)) {
    prv_respond_malformed_command(cmd, length, NULL);
    return;
  }

  switch (cmd->region.region) {
    case FLASH_REGION_PRF:
      flash_prf_set_protection(true);
      break;
    case FLASH_REGION_SYSTEM_RESOURCES:
      boot_bit_set(BOOT_BIT_NEW_SYSTEM_RESOURCES_AVAILABLE);
      break;
    default:
      break;
  }

  cmd->region.opcode = IMAGING_RESP_FINALIZE_REGION;

  Command *resp = pulse_best_effort_send_begin(PULSE_PROTOCOL_FLASH_IMAGING);
  resp->region.opcode = cmd->region.opcode;
  resp->region.region = cmd->region.region;

  pulse_best_effort_send(resp, sizeof(resp->region));
}

static void prv_respond_malformed_command(Command *cmd, size_t length,
                                          const char *message) {
  typedef struct PACKED MalformedCommandResponse {
    uint8_t opcode;
    uint8_t bad_command[9];
    char error_message[40];
  } MalformedCommandResponse;

  MalformedCommandResponse *resp = pulse_best_effort_send_begin(
      PULSE_PROTOCOL_FLASH_IMAGING);
  resp->opcode = IMAGING_RESP_MALFORMED_CMD;

  memcpy(resp->bad_command, cmd, MIN(length, sizeof(resp->bad_command)));
  size_t response_length = sizeof(resp->opcode) + sizeof(resp->bad_command);
  if (message) {
    for (size_t i = 0; i < sizeof(resp->error_message); ++i) {
      if (message[i] == '\0') {
        break;
      }
      resp->error_message[i] = message[i];
      response_length += 1;
    }
  }
  pulse_best_effort_send(resp, response_length);
}
