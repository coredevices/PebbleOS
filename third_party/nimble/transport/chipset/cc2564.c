/*
 * Copyright 2025 Google LLC
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

#include "board/board.h"
#include "drivers/gpio.h"
#include "kernel/util/sleep.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "resource/resource_mapped.h"

typedef struct PACKED {
  uint8_t type;
  uint16_t opcode;
  uint8_t size;
} BTSHCICommand;

extern void ble_queue_cmd(void *buf, bool needs_free, bool wait);

static bool ble_run_bts(const ResAppNum bts_file) {
  size_t i = 0;
  size_t bts_len = 0;

  if (!resource_is_valid(SYSTEM_APP, bts_file)) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "Can't load BT service pack: bad system resources!");
    return false;
  }

  PebbleTask task = pebble_task_get_current();
  resource_mapped_use(task);

  const uint8_t *bts_data =
      resource_get_readonly_bytes(SYSTEM_APP, bts_file, &bts_len, true /* is_privileged */);

  while (i < bts_len) {
    BTSHCICommand *command = (BTSHCICommand *)&bts_data[i];
    i += sizeof(BTSHCICommand) + command->size;

    // skip update baud rate command and sleep mode config
    // TODO: re-add sleep mode config and deal with entering/exiting sleep mode
    if (command->opcode == 0xFF36 || command->opcode == 0xFD0C) {
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_INFO, "ble_bts: Skipping opcode 0x%X", command->opcode);
      continue;
    }
    ble_queue_cmd(&command->opcode, false, true);
  }

  resource_mapped_release(task);

  return true;
}

void ble_chipset_init(void) {
  gpio_output_init(&BOARD_CONFIG_BT_COMMON.reset, GPIO_OType_PP, GPIO_Speed_25MHz);
  gpio_output_set(&BOARD_CONFIG_BT_COMMON.reset, true);
  psleep(100);
  gpio_output_set(&BOARD_CONFIG_BT_COMMON.reset, false);
}

bool ble_chipset_start(void) {
  if (!ble_run_bts(RESOURCE_ID_BT_PATCH)) return false;

  // HACK: this is just here to let the service pack commands get processed before we continue
  psleep(500);

  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_INFO, "bts files sent");

  return true;
}
