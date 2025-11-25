/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "comm/ble/gatt_client_subscriptions.h"

#include <stdint.h>

#include "util/uuid.h"
#include "mfg/mfg_serials.h"
#include "util/attributes.h"

#define PPOGATT_V1_DESIRED_RX_WINDOW_SIZE (4500 / MAX_ATT_WRITE_PAYLOAD_SIZE)
#define PPOGATT_MIN_VERSION (0x00)
#define PPOGATT_MAX_VERSION (0x01)
#define PPOGATT_SN_BITS (5)
#define PPOGATT_SN_MOD_DIV (1 << PPOGATT_SN_BITS)
#define PPOGATT_V0_WINDOW_SIZE (4)
#define PPOGATT_TIMEOUT_TICK_INTERVAL_SECS (2)
//! Effective timeout: between 5 - 6 secs, because packet could be sent out just before the
//! RegularTimer second tick is about to fire.
#define PPOGATT_TIMEOUT_TICKS (3)

//! Number of maximum consecutive timeouts without getting a packet Ack'd
#define PPOGATT_TIMEOUT_COUNT_MAX (2)
//! Number of maximum consecutive resets without getting a packet Ack'd
#define PPOGATT_RESET_COUNT_MAX (5)
//! Number of maximum consecutive disconnects without getting a packet Ack'd
#define PPOGATT_DISCONNECT_COUNT_MAX (2)
//! Maximum amount of time PPoGATT will wait before sending an Ack for received data
#define PPOGATT_MAX_DATA_ACK_LATENCY_MS (200)

typedef enum {
  PPoGATTPacketTypeData = 0x0,
  PPoGATTPacketTypeAck = 0x1,
  PPoGATTPacketTypeResetRequest = 0x2,
  PPoGATTPacketTypeResetComplete = 0x3,
  PPoGATTPacketTypeInvalidRangeStart,
} PPoGATTPacketType;

_Static_assert(PPoGATTPacketTypeAck != 0, "Ack type can't be 0; see ack_packet_byte");
_Static_assert(PPoGATTPacketTypeResetRequest != 0, "Reset type can't be 0; see reset_packet_byte");
_Static_assert(PPoGATTPacketTypeResetComplete != 0, "Reset type can't be 0; see reset_packet_byte");

typedef struct PACKED {
  PPoGATTPacketType type:3;
  uint8_t sn:PPOGATT_SN_BITS;
  uint8_t payload[];
} PPoGATTPacket;

_Static_assert(sizeof(PPoGATTPacket) == 1,
               "You can't increase the size of PPoGATTPacket. It's set in stone now!");

//! Client identification payload that is attached to the client's Reset Request messages
typedef struct PACKED {
  //! The PPoGATT version that the client wants to use.
  //! Must be within the server's [ppogatt_min_version, ppogatt_max_version]
  uint8_t ppogatt_version;

  //! The serial number of the client device.
  char serial_number[MFG_SERIAL_NUMBER_SIZE];
} PPoGATTResetRequestClientIDPayload;

typedef struct PACKED {
  uint8_t ppogatt_max_rx_window;
  uint8_t ppogatt_max_tx_window;
} PPoGATTResetCompleteClientIDPayloadV1;

typedef struct PACKED {
  uint8_t ppogatt_min_version;
  uint8_t ppogatt_max_version;
  Uuid app_uuid;
} PPoGATTMetaV0;

typedef enum {
  PPoGATTSessionType_InferredFromUuid = 0x00,
  PPoGATTSessionType_Hybrid = 0x01,
  PPoGATTSessionTypeCount,
} PPoGATTSessionType;

typedef struct PACKED {
  uint8_t ppogatt_min_version;
  uint8_t ppogatt_max_version;
  Uuid app_uuid;
  PPoGATTSessionType pp_session_type:8;
} PPoGATTMetaV1;
