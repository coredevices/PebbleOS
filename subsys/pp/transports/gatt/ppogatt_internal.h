/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/pp/transports/gatt.h>
#include <pbl/util/uuid.h>

//! Service of the forward role, hosted by the phone, and its characteristics. They are 32-bit
//! values expanded with PPOGATT_UUID128_INIT().
#define PPOGATT_SERVICE_UUID             (0x10000000)
#define PPOGATT_DATA_CHARACTERISTIC_UUID (0x10000001)
#define PPOGATT_META_CHARACTERISTIC_UUID (0x10000002)
// 0x30000003 to 0x30000006 are taken: an earlier service hosted by the device used them.
//! Service of the reversed role, hosted by the device: the phone writes one characteristic and
//! is notified of the other. There is no meta characteristic, 0x40000002 is reserved for one.
#define PPOGATT_REVERSED_SERVICE_UUID                (0x40000000)
#define PPOGATT_REVERSED_DATA_CHARACTERISTIC_UUID    (0x40000001)
#define PPOGATT_REVERSED_DATA_WR_CHARACTERISTIC_UUID (0x40000003)

//! Initializer of a ble_uuid128_t: a 32-bit UUID expanded with the base UUID,
//! XXXXXXXX-328E-0FBB-C642-1AA6699BDADA.
#define PPOGATT_UUID128_INIT(u)                                                            \
  BLE_UUID128_INIT(0xDA, 0xDA, 0x9B, 0x69, 0xA6, 0x1A, 0x42, 0xC6, 0xBB, 0x0F, 0x8E, 0x32, \
                   (u) & 0xff, ((u) >> 8) & 0xff, ((u) >> 16) & 0xff, ((u) >> 24) & 0xff)

//! The largest ATT MTU: what the host stack is configured to prefer.
#define PPOGATT_MTU_MAX                (247)
#define PPOGATT_DESIRED_RX_WINDOW_SIZE (4500 / (PPOGATT_MTU_MAX - 3))
//! The smallest ATT MTU: anything below means that the connection is gone.
#define PPOGATT_MTU_MIN (23)
//! The one version spoken.
#define PPOGATT_VERSION    (0x01)
#define PPOGATT_SN_BITS    (5)
#define PPOGATT_SN_MOD_DIV (1 << PPOGATT_SN_BITS)
//! Window used when the phone does not tell its own.
#define PPOGATT_FALLBACK_WINDOW_SIZE       (4)
#define PPOGATT_TIMEOUT_TICK_INTERVAL_SECS (2)
//! Effective timeout: between 5 - 6 secs, because packet could be sent out just before the
//! timer tick is about to fire.
#define PPOGATT_TIMEOUT_TICKS (3)

//! Number of maximum consecutive timeouts without getting a packet Ack'd
#define PPOGATT_TIMEOUT_COUNT_MAX (2)
//! Number of maximum consecutive resets without getting a packet Ack'd
#define PPOGATT_RESET_COUNT_MAX (5)
//! Number of maximum consecutive disconnects without getting a packet Ack'd
#define PPOGATT_DISCONNECT_COUNT_MAX (2)
//! Maximum amount of time PPoGATT will wait before sending an Ack for received data
#define PPOGATT_MAX_DATA_ACK_LATENCY_MS (200)

//! Number of maximum retries for reading the meta characteristic
#define PPOGATT_META_READ_RETRY_COUNT_MAX (3)
//! Delay in milliseconds before retrying a failed meta characteristic read
#define PPOGATT_META_READ_RETRY_DELAY_MS (500)

//! Delay in milliseconds before the first retry of a send that failed for lack
//! of BT stack buffers (NimBLE has no "buffers freed" event)
#define PPOGATT_SEND_RETRY_DELAY_MS (20)
//! Each further retry doubles the delay, up to this
#define PPOGATT_SEND_RETRY_MAX_DELAY_MS (200)

typedef enum {
  //! Watch is the GATT client; phone hosts the PPoG service.
  PPoGATTRoleForward,
  //! Watch hosts the PPoG service; phone is the GATT client.
  PPoGATTRoleReversed,
} PPoGATTRole;

typedef enum {
  PPoGATTPacketTypeData = 0x0,
  PPoGATTPacketTypeAck = 0x1,
  PPoGATTPacketTypeResetRequest = 0x2,
  PPoGATTPacketTypeResetComplete = 0x3,
  PPoGATTPacketTypeInvalidRangeStart,
} PPoGATTPacketType;

static_assert(PPoGATTPacketTypeAck != 0, "Ack type can't be 0; see ack_packet_byte");
static_assert(PPoGATTPacketTypeResetRequest != 0, "Reset type can't be 0; see reset_packet_byte");
static_assert(PPoGATTPacketTypeResetComplete != 0, "Reset type can't be 0; see reset_packet_byte");

typedef struct PBL_PACKED {
  PPoGATTPacketType type : 3;
  uint8_t sn : PPOGATT_SN_BITS;
  uint8_t payload[];
} PPoGATTPacket;

//! PPoGATTPacket without payload
typedef struct PBL_PACKED {
  PPoGATTPacketType type : 3;
  uint8_t sn : PPOGATT_SN_BITS;
} PPoGATTPacketHeader;

static_assert(sizeof(PPoGATTPacketHeader) == sizeof(PPoGATTPacket), "");

static_assert(sizeof(PPoGATTPacket) == 1,
              "You can't increase the size of PPoGATTPacket. It's set in stone now!");

//! Client identification payload that is attached to the client's Reset Request messages
typedef struct PBL_PACKED {
  //! The PPoGATT version that the client wants to use.
  //! Must be within the server's [ppogatt_min_version, ppogatt_max_version]
  uint8_t ppogatt_version;

  //! The serial number of the client device.
  char serial_number[PBL_PP_GATT_SERIAL_NUMBER_SIZE];
} PPoGATTResetRequestClientIDPayload;

typedef struct PBL_PACKED {
  uint8_t ppogatt_max_rx_window;
  uint8_t ppogatt_max_tx_window;
} PPoGATTResetCompleteClientIDPayload;

typedef enum {
  PPoGATTSessionType_System = 0x00,
  PPoGATTSessionType_Hybrid = 0x01,
  PPoGATTSessionTypeCount,
} PPoGATTSessionType;

typedef struct PBL_PACKED {
  uint8_t ppogatt_min_version;
  uint8_t ppogatt_max_version;
  Uuid app_uuid;
  PPoGATTSessionType pp_session_type : 8;
} PPoGATTMeta;

//! The reversed service, where the phone is the client (CONFIG_PP_TRANSPORT_GATT_REVERSED).

//! Notify a packet to the phone.
//! @return 0, -ENOMEM if there is no buffer for it now, -ENOTCONN if the connection is gone,
//! another negative errno otherwise.
int ppogatt_reversed_service_notify(uint16_t conn_handle, const uint8_t *buf, uint16_t len);

//! What the service reports, on the NimBLE host task.
void ppogatt_reversed_service_subscribed(uint16_t conn_handle);
void ppogatt_reversed_service_unsubscribed(uint16_t conn_handle);
//! @param buf The packet, on the kernel heap: freed by the callee.
void ppogatt_reversed_service_data_written(uint16_t conn_handle, uint8_t *buf, uint16_t len);
