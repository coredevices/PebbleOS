/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/endpoint.h>
#include <pbl/services/bluetooth/pairability.h>
#include <pbl/util/byteorder.h>

PBL_LOG_MODULE_DECLARE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

// Opcodes 0 to 3 were used once.
#define OPCODE_SET_DISCOVERABLE_PAIRABLE 4U

struct PBL_PACKED set_discoverable_pairable {
  uint8_t opcode;
  uint8_t discoverable_pairable;
  // Seconds, little endian.
  uint16_t duration;
};

static void prv_msg_cb(struct pbl_pp_session *session, const uint8_t *data, size_t length) {
  const struct set_discoverable_pairable *cmd = (const struct set_discoverable_pairable *)data;
  uint16_t duration;

  if (length < sizeof(*cmd)) {
    PBL_LOG_WRN("Short BLE control message: %d", (int)length);
    return;
  }

  if (cmd->opcode != OPCODE_SET_DISCOVERABLE_PAIRABLE) {
    PBL_LOG_WRN("Unsupported BLE control opcode: %u", cmd->opcode);
    return;
  }

  duration = pbl_le16_to_cpu(cmd->duration);
  PBL_LOG_DBG("Set discoverable pairable: %u, %u s", cmd->discoverable_pairable, duration);

  bt_pairability_use_for_period(cmd->discoverable_pairable != 0U ? duration : 0U);
}

PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_BLE_CONTROL, PBL_PP_ACCESS_PRIVATE, prv_msg_cb);
