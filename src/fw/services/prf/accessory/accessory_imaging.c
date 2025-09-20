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

#include "accessory_imaging.h"

#include "console/prompt.h"
#include "drivers/accessory.h"
#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "kernel/core_dump.h"
#include "kernel/core_dump_private.h"
#include "mfg/mfg_mode/mfg_factory_mode.h"
#include "resource/resource_storage_flash.h"
#include "services/common/new_timer/new_timer.h"
#include "services/common/system_task.h"
#include "services/prf/accessory/accessory_manager.h"
#include "system/bootbits.h"
#include "system/logging.h"
#include "system/passert.h"
#include "system/reset.h"
#include "util/crc32.h"
#include "util/hdlc.h"
#include "util/attributes.h"
#include "util/math.h"

#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#define TIMEOUT_MS (3000)
#define VERSION (1)
#define NUM_RX_BUFFERS (3)
#define MAX_DATA_LENGTH (2048)
#define CHECKSUM_LENGTH (4)
#define MAX_FRAME_LENGTH (MAX_DATA_LENGTH + sizeof(ImagingHeader) + CHECKSUM_LENGTH)

// flags
#define FLAG_IS_SERVER (1 << 0)
#define FLAG_VERSION (VERSION << 1)

// opcodes
#define OPCODE_PING (0x01)
#define OPCODE_DISCONNECT (0x02)
#define OPCODE_RESET (0x03)
#define OPCODE_FLASH_GEOMETRY (0x11)
#define OPCODE_FLASH_ERASE (0x12)
#define OPCODE_FLASH_WRITE (0x13)
#define OPCODE_FLASH_CRC (0x14)
#define OPCODE_FLASH_FINALIZE (0x15)
#define OPCODE_FLASH_READ (0x16)

// flash regions
#define REGION_PRF (0x01)
#define REGION_RESOURCES (0x02)
#define REGION_FW_SCRATCH (0x03)
#define REGION_PFS (0x04)
#define REGION_COREDUMP (0x05)

// flash read flags
#define FLASH_READ_FLAG_ALL_SAME (1 << 0)

typedef struct PACKED {
  uint8_t flags;
  uint8_t opcode;
} ImagingHeader;

typedef struct PACKED {
  uint8_t region;
} FlashGeometryRequest;

typedef struct PACKED {
  uint8_t region;
  uint32_t address;
  uint32_t length;
} FlashGeometryResponse;

typedef struct PACKED {
  uint32_t address;
  uint32_t length;
} FlashEraseRequest;

typedef struct PACKED {
  uint32_t address;
  uint32_t length;
  uint8_t complete;
} FlashEraseResponse;

typedef struct PACKED {
  uint32_t address;
  uint8_t data[];
} FlashWriteRequest;

typedef struct PACKED {
  uint32_t address;
  uint32_t length;
} FlashReadRequest;

typedef struct PACKED {
  uint32_t address;
  uint32_t length;
} FlashCRCRequest;

typedef struct PACKED {
  uint32_t address;
  uint32_t length;
  uint32_t crc;
} FlashCRCResponse;

typedef struct PACKED {
  uint8_t region;
} FlashFinalizeRequest;

typedef FlashFinalizeRequest FlashFinalizeResponse; // they are currently the same

typedef struct {
  bool is_free;
  bool is_valid;
  HdlcStreamingContext hdlc_ctx;
  uint32_t index;
  union {
    struct PACKED {
      ImagingHeader header;
      uint8_t payload[MAX_DATA_LENGTH];
      uint32_t checksum;
    } frame;
    uint8_t data[MAX_FRAME_LENGTH];
  };
  uint32_t checksum;
} ReceiveBuffer;

static bool s_enabled;
static TimerID s_timeout_timer;
static ReceiveBuffer s_buffers[NUM_RX_BUFFERS];
static ReceiveBuffer *s_curr_buf;
static bool s_flash_erase_in_progress;
static int s_no_buffer_count;
static int s_dropped_char_count;

static void prv_timeout_timer_cb(void *context);


// Helper functions
////////////////////////////////////////////////////////////////////

static void prv_reset_buffer(ReceiveBuffer *buffer) {
  hdlc_streaming_decode_reset(&buffer->hdlc_ctx);
  buffer->index = 0;
  buffer->checksum = CRC32_INIT;
  buffer->is_valid = true;
  buffer->is_free = true;
}


// Start / stop
////////////////////////////////////////////////////////////////////

static void prv_start(void) {
  s_curr_buf = NULL;
  s_no_buffer_count = 0;
  s_dropped_char_count = 0;
  for (int i = 0; i < NUM_RX_BUFFERS; ++i) {
    prv_reset_buffer(&s_buffers[i]);
  }
  accessory_manager_set_state(AccessoryInputStateImaging);
  accessory_use_dma(true);
  s_timeout_timer = new_timer_create();
  new_timer_start(s_timeout_timer, TIMEOUT_MS, prv_timeout_timer_cb, NULL, 0 /* flags */);
  PBL_LOG(LOG_LEVEL_DEBUG, "Starting accessory imaging");
}

static void prv_stop(void *context) {
  if (s_no_buffer_count > 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "Ran out of buffers %d times and dropped %d bytes while imaging",
            s_no_buffer_count, s_dropped_char_count);
  }
  flash_prf_set_protection(true);
  accessory_use_dma(false);
  accessory_manager_set_state(AccessoryInputStateMfg);
  new_timer_delete(s_timeout_timer);
  PBL_LOG(LOG_LEVEL_DEBUG, "Stopping accessory imaging");
}

static void prv_timeout_timer_cb(void *context) {
  system_task_add_callback(prv_stop, NULL);
}


// Sending
////////////////////////////////////////////////////////////////////

static void prv_encode_and_send_data(const void *data, uint32_t length) {
  const uint8_t *data_bytes = data;
  for (uint32_t i = 0; i < length; i++) {
    uint8_t byte = data_bytes[i];
    if (hdlc_encode(&byte)) {
      accessory_send_byte(HDLC_ESCAPE);
    }
    accessory_send_byte(byte);
  }
}

static void prv_send_frame(uint8_t opcode, const void *payload, uint32_t length) {
  accessory_disable_input();
  accessory_send_byte(HDLC_FLAG);

  // send the header
  const ImagingHeader header = {
    .flags = FLAG_IS_SERVER | FLAG_VERSION,
    .opcode = opcode
  };
  prv_encode_and_send_data(&header, sizeof(header));

  // send the pyaload
  prv_encode_and_send_data(payload, length);

  // send the checksum
  uint32_t checksum = CRC32_INIT;
  checksum = crc32(checksum, &header, sizeof(header));
  if (payload && length) {
    checksum = crc32(checksum, payload, length);
  }
  prv_encode_and_send_data(&checksum, sizeof(checksum));

  accessory_send_byte(HDLC_FLAG);
  accessory_enable_input();
}


// Request processing
////////////////////////////////////////////////////////////////////

static void prv_erase_complete(void *ignored, status_t result) {
  s_flash_erase_in_progress = false;
}

static bool prv_is_erased(uint32_t addr, uint32_t length) {
  const uint32_t sectors_to_erase = (length + SECTOR_SIZE_BYTES - 1) / SECTOR_SIZE_BYTES;
  for (uint32_t sector = 0; sector < sectors_to_erase; sector++) {
    if (!flash_sector_is_erased(sector * SECTOR_SIZE_BYTES + addr)) {
      return false;
    }
  }
  return true;
}

static void prv_handle_ping_request(const void *payload, uint32_t length) {
  // echo it back
  prv_send_frame(OPCODE_PING, payload, length);
}

static void prv_handle_disconnect_request(const void *payload, uint32_t length) {
  if (length) {
    // should be 0
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid length (%"PRIu32")", length);
    return;
  }

  prv_send_frame(OPCODE_DISCONNECT, NULL, 0);
  prv_stop(NULL);
}

static void prv_handle_reset_request(const void *payload, uint32_t length) {
  if (length) {
    // should be 0
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid length (%"PRIu32")", length);
    return;
  }

  PBL_LOG(LOG_LEVEL_WARNING, "Got reset request");
  prv_send_frame(OPCODE_RESET, NULL, 0);
  prv_stop(NULL);
  system_reset();
}

static bool prv_coredump_flash_base(uint32_t *addr, uint32_t *size) {
  CoreDumpFlashHeader flash_hdr;
  CoreDumpFlashRegionHeader region_hdr;
  uint32_t max_last_used = 0;
  uint32_t base_address;
  uint32_t last_used_idx = 0;

  // First, see if the flash header has been put in place
  flash_read_bytes((uint8_t *)&flash_hdr, CORE_DUMP_FLASH_START, sizeof(flash_hdr));

  if ((flash_hdr.magic != CORE_DUMP_FLASH_HDR_MAGIC) ||
      (flash_hdr.unformatted == CORE_DUMP_ALL_UNFORMATTED)) {
    return false;
  }

  // Find the region with the highest last_used count
  for (unsigned int i = 0; i < CORE_DUMP_MAX_IMAGES; i++) {
    if (flash_hdr.unformatted & (1 << i)) {
      continue;
    }

    base_address = core_dump_get_slot_address(i);
    flash_read_bytes((uint8_t *)&region_hdr, base_address, sizeof(region_hdr));

    if (region_hdr.last_used > max_last_used) {
      max_last_used = region_hdr.last_used;
      last_used_idx = i;
    }
  }

  if (max_last_used == 0) {
    return false;
  }

  *addr = core_dump_get_slot_address(last_used_idx);
  if (core_dump_size(*addr, size) != S_SUCCESS) {
    return false;
  }
  *addr += sizeof(CoreDumpFlashRegionHeader);
  return true;
}

static void prv_handle_flash_geometry_request(const void *payload, uint32_t length) {
  if (length != sizeof(FlashGeometryRequest)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid length (%"PRIu32")", length);
    return;
  }

  const FlashGeometryRequest *request = payload;

  FlashGeometryResponse response = {
    .region = request->region
  };
  if (request->region == REGION_PRF) {
    // assume we're about to write to this region, so unlock it
    flash_prf_set_protection(false);
    response.address = FLASH_REGION_SAFE_FIRMWARE_BEGIN;
    response.length = FLASH_REGION_SAFE_FIRMWARE_END - response.address;
  } else if (request->region == REGION_RESOURCES) {
    const SystemResourceBank *bank = resource_storage_flash_get_unused_bank();
    response.address = bank->begin;
    response.length = bank->end - response.address;
  } else if (request->region == REGION_FW_SCRATCH) {
    response.address = FLASH_REGION_FIRMWARE_DEST_BEGIN;
    response.length = FLASH_REGION_FIRMWARE_DEST_END - response.address;
  } else if (request->region == REGION_PFS) {
    response.address = FLASH_REGION_FILESYSTEM_BEGIN;
    response.length = FLASH_REGION_FILESYSTEM_END - response.address;
  } else if (request->region == REGION_COREDUMP) {
    if (!prv_coredump_flash_base(&response.address, &response.length)) {
      response.address = 0;
      response.length = 0;
    }
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid region (%"PRIu8")", request->region);
  }
  prv_send_frame(OPCODE_FLASH_GEOMETRY, &response, sizeof(response));
}

static void prv_handle_flash_erase_request(const void *payload, uint32_t length) {
  if (length != sizeof(FlashEraseRequest)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid length (%"PRIu32")", length);
    return;
  }

  const FlashEraseRequest *request = payload;

  FlashEraseResponse response = {
    .address = request->address,
    .length = request->length
  };
  bool start_erase = false;
  if (s_flash_erase_in_progress) {
    response.complete = 0;
  } else if (prv_is_erased(request->address, request->length)) {
    response.complete = 1;
  } else {
    response.complete = 0;
    start_erase = true;
  }
  prv_send_frame(OPCODE_FLASH_ERASE, &response, sizeof(response));

  // start the erase after sending the response
  if (start_erase) {
    uint32_t end_address = request->address + request->length;
    s_flash_erase_in_progress = true;
    flash_erase_optimal_range(
        request->address, request->address, end_address,
        (end_address + SECTOR_SIZE_BYTES - 1) & SECTOR_ADDR_MASK,
        prv_erase_complete, NULL);
  }
}

static void prv_handle_flash_write_request(const void *payload, uint32_t length) {
  if (length < sizeof(FlashWriteRequest)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid length (%"PRIu32")", length);
    return;
  }

  const FlashWriteRequest *request = payload;
  length -= offsetof(FlashWriteRequest, data);

  flash_write_bytes(request->data, request->address, length);
}

static void prv_handle_flash_read_request(const void *payload, uint32_t length) {
  if (length < sizeof(FlashReadRequest)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid length (%"PRIu32")", length);
    return;
  }

  const FlashReadRequest *request = payload;
  if (request->length > MAX_DATA_LENGTH) {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid request length (%"PRIu32")", request->length);
  }

  // leave 1 byte at the start for flags
  static uint8_t buffer[1 + MAX_DATA_LENGTH];
  flash_read_bytes(&buffer[1], request->address, request->length);
  bool is_all_same = true;
  uint8_t same_byte = buffer[1];
  for (uint32_t i = 1; i < request->length; i++) {
    if (buffer[i + 1] != same_byte) {
      is_all_same = false;
      break;
    }
  }
  // As an optimization, if all the bytes are the same, we set a flag and just send a single byte.
  buffer[0] = is_all_same ? FLASH_READ_FLAG_ALL_SAME : 0; // flags
  const uint32_t frame_length = is_all_same ? 2 : request->length + 1;
  prv_send_frame(OPCODE_FLASH_READ, buffer, frame_length);
}

static void prv_handle_flash_crc_request(const void *payload, uint32_t length) {
  // there can be 1 or more payloads
  if (!length || (length % sizeof(FlashCRCRequest))) {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid length (%"PRIu32")", length);
    return;
  }

  const FlashCRCRequest *request = payload;
  const uint32_t num_entries = length / sizeof(FlashCRCRequest);

  // this is just static cause it's potentially too big to put on the stack
  static FlashCRCResponse response[MAX_DATA_LENGTH / sizeof(FlashCRCResponse)];
  for (uint32_t i = 0; i < num_entries; i++) {
    const FlashCRCRequest *entry = &request[i];
    response[i] = (FlashCRCResponse) {
      .address = entry->address,
      .length = entry->length,
      .crc = flash_crc32(entry->address, entry->length)
    };
  }

  prv_send_frame(OPCODE_FLASH_CRC, &response, sizeof(FlashCRCResponse) * num_entries);
}

static void prv_handle_flash_finalize_request(const void *payload, uint32_t length) {
  if (length != sizeof(FlashFinalizeRequest)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid length (%"PRIu32")", length);
    return;
  }

  const FlashFinalizeRequest *request = payload;

  FlashFinalizeResponse response = {
    .region = request->region
  };
  if (request->region == REGION_PRF) {
    flash_prf_set_protection(true);
  } else if (request->region == REGION_RESOURCES) {
    boot_bit_set(BOOT_BIT_NEW_SYSTEM_RESOURCES_AVAILABLE);
  } else if (request->region == REGION_FW_SCRATCH) {
    boot_bit_set(BOOT_BIT_NEW_FW_AVAILABLE);
  } else if (request->region == REGION_PFS) {
    // Do nothing!
  } else if (request->region == REGION_COREDUMP) {
    // Do nothing!
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid region (%"PRIu8")", request->region);
  }
  prv_send_frame(OPCODE_FLASH_FINALIZE, &response, sizeof(response));
}

static void prv_process_frame(void *context) {
  ReceiveBuffer *buf = context;
  const ImagingHeader *header = &buf->frame.header;
  const void *payload = buf->frame.payload;
  const uint32_t payload_length = buf->index - sizeof(ImagingHeader) - CHECKSUM_LENGTH;
  PBL_ASSERTN(payload_length <= MAX_DATA_LENGTH);

  // sanity check
  if (header->flags & FLAG_IS_SERVER) {
    PBL_LOG(LOG_LEVEL_ERROR, "Got frame from server (loopback?)");
    prv_reset_buffer(buf);
    return;
  }

  // reset the timeout timer
  new_timer_start(s_timeout_timer, TIMEOUT_MS, prv_timeout_timer_cb, NULL, 0 /* flags */);

  // look at the opcode and handle this message
  if (header->opcode == OPCODE_PING) {
    prv_handle_ping_request(payload, payload_length);
  } else if (header->opcode == OPCODE_DISCONNECT) {
    prv_handle_disconnect_request(payload, payload_length);
  } else if (header->opcode == OPCODE_RESET) {
    prv_handle_reset_request(payload, payload_length);
  } else if (header->opcode == OPCODE_FLASH_GEOMETRY) {
    prv_handle_flash_geometry_request(payload, payload_length);
  } else if (header->opcode == OPCODE_FLASH_ERASE) {
    prv_handle_flash_erase_request(payload, payload_length);
  } else if (header->opcode == OPCODE_FLASH_WRITE) {
    prv_handle_flash_write_request(payload, payload_length);
  } else if (header->opcode == OPCODE_FLASH_READ) {
    prv_handle_flash_read_request(payload, payload_length);
  } else if (header->opcode == OPCODE_FLASH_CRC) {
    prv_handle_flash_crc_request(payload, payload_length);
  } else if (header->opcode == OPCODE_FLASH_FINALIZE) {
    prv_handle_flash_finalize_request(payload, payload_length);
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "Got unexpected opcode (0x%x)", header->opcode);
  }

  prv_reset_buffer(buf);
}


// Receiving (ISR-based)
////////////////////////////////////////////////////////////////////

static bool prv_handle_data(uint8_t data) {
  bool should_context_switch = false;
  bool hdlc_err;
  bool should_store;
  bool is_complete = hdlc_streaming_decode(&s_curr_buf->hdlc_ctx, &data, &should_store, &hdlc_err);
  if (hdlc_err) {
    s_curr_buf->is_valid = false;
  } else if (is_complete) {
    if (s_curr_buf->is_valid && s_curr_buf->checksum == CRC32_RESIDUE && s_curr_buf->index) {
      // queue up processing of this frame and clear s_curr_buf so we'll switch to a new one
      system_task_add_callback_from_isr(prv_process_frame, s_curr_buf, &should_context_switch);
    } else {
      prv_reset_buffer(s_curr_buf);
    }
    s_curr_buf = NULL;
  } else if (should_store && s_curr_buf->is_valid) {
    if (s_curr_buf->index < MAX_FRAME_LENGTH) {
      // store this byte
      s_curr_buf->data[(s_curr_buf->index)++] = data;
      s_curr_buf->checksum = crc32(s_curr_buf->checksum, &data, 1);
    } else {
      // too long!
      s_curr_buf->is_valid = false;
    }
  }
  return should_context_switch;
}

bool accessory_imaging_handle_char(char c) {
  static bool has_no_buffer = false;
  if (!s_curr_buf) {
    // find a buffer to write into
    for (int i = 0; i < NUM_RX_BUFFERS; ++i) {
      if (s_buffers[i].is_free) {
        s_buffers[i].is_free = false;
        s_curr_buf = &s_buffers[i];
        break;
      }
    }
    if (!s_curr_buf) {
      // no available buffer :(
      if (!has_no_buffer) {
        s_no_buffer_count++;
      }
      has_no_buffer = true;
      s_dropped_char_count++;
      return false;
    }
  }
  has_no_buffer = false;

  return prv_handle_data((uint8_t)c);
}


// Other exported functions
////////////////////////////////////////////////////////////////////

void accessory_imaging_enable(void) {
  s_enabled = true;
}

void command_accessory_imaging_start(void) {
  if (!s_enabled) {
    prompt_send_response("Command not available.");
  }
  prv_start();
}
