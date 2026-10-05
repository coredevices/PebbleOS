/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/mutex.h"
#include "pbl/kernel/compiler.h"
#include "pbl/util/shared_cbuf.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/**
 * @defgroup comm_qemu_serial_private Channel internals
 * @ingroup comm_qemu
 * @brief QEMU channel framing and receive state machine, shared with the unit tests.
 * @{
 */

/** @brief Frame header signature. */
#define QEMU_HEADER_SIGNATURE 0xFEED
/** @brief Frame footer signature. */
#define QEMU_FOOTER_SIGNATURE 0xBEEF
/** @brief Maximum payload length in bytes. */
#define QEMU_MAX_DATA_LEN 2048

/**
 * @brief Frame header.
 *
 * All fields are in network byte order.
 */
typedef struct PBL_PACKED {
  /** @ref QEMU_HEADER_SIGNATURE. */
  uint16_t signature;
  /** Protocol, a @ref QemuProtocol. */
  uint16_t protocol;
  /** Payload length in bytes, excluding header and footer; at most @ref QEMU_MAX_DATA_LEN. */
  uint16_t len;
} QemuCommChannelHdr;

/** @brief Frame footer, in network byte order. */
typedef struct PBL_PACKED {
  /** @ref QEMU_FOOTER_SIGNATURE. */
  uint16_t signature;
} QemuCommChannelFooter;

/**
 * @brief Incoming message handler.
 *
 * @param data Payload.
 * @param length Length of @p data in bytes.
 */
typedef void (*QemuMessageCallback)(const uint8_t *data, uint32_t length);

/** @brief Binding of a protocol to its incoming message handler. */
typedef struct {
  /** Protocol, a @ref QemuProtocol. */
  uint16_t protocol_id;
  /** Handler. */
  QemuMessageCallback callback;
} QemuMessageHandler;

/** @brief Receive state machine state. */
typedef enum {
  /** Waiting for the first header signature byte. */
  QemuRecvState_WaitingHdrSignatureMSB,
  /** Waiting for the second header signature byte. */
  QemuRecvState_WaitingHdrSignatureLSB,
  /** Receiving the rest of the header. */
  QemuRecvState_WaitingHdr,
  /** Receiving the payload. */
  QemuRecvState_WaitingData,
  /** Receiving the footer. */
  QemuRecvState_WaitingFooter,
} QemuRecvState;

/** @brief QEMU channel state. */
typedef struct {
  /** State initialized. */
  bool initialized;
  /** Serializes sends. */
  struct pbl_mutex qemu_comm_lock;
  /** Bytes received by the UART ISR. */
  struct pbl_shared_cbuf isr_buffer;
  /** Reader of @ref isr_buffer. */
  struct pbl_shared_cbuf_client isr_buffer_client;

  /** Receive state machine state. */
  QemuRecvState recv_state;
  /** Previously received byte. */
  uint8_t prev_byte;
  /** Header of the frame being received. */
  QemuCommChannelHdr hdr;
  /** Payload of the frame being received, @ref QEMU_MAX_DATA_LEN bytes. */
  uint8_t *msg_buffer;
  /** Payload bytes received so far. */
  uint16_t msg_buffer_bytes;
  /** A buffer processing callback is scheduled. */
  bool callback_pending;
  /** Receive errors since the last report. */
  uint32_t recv_error_count;
  /** Time the current frame started, in seconds. */
  time_t start_recv_packet_time;
} QemuSerialGlobals;

/** @brief QEMU UART baud rate. */
#define UART_SERIAL_BAUD_RATE 115200

/** @brief ISR receive buffer headroom in bytes beyond a maximum-size payload. */
#define QEMU_ISR_RECV_HIGH_WATER_DELTA (128)
/** @brief ISR receive buffer size in bytes. */
#define QEMU_ISR_RECV_BUFFER_SIZE (QEMU_MAX_DATA_LEN + QEMU_ISR_RECV_HIGH_WATER_DELTA)
/** @brief Time in seconds to receive a complete frame before the receiver resets. */
#define QEMU_RECV_PACKET_TIMEOUT_SEC 10

/** @brief Most significant byte of @ref QEMU_FOOTER_SIGNATURE. */
#define QEMU_FOOTER_MSB ((uint8_t)(QEMU_FOOTER_SIGNATURE >> 8))
/** @brief Least significant byte of @ref QEMU_FOOTER_SIGNATURE. */
#define QEMU_FOOTER_LSB ((uint8_t)(QEMU_FOOTER_SIGNATURE & 0x00FF))
/** @brief Most significant byte of @ref QEMU_HEADER_SIGNATURE. */
#define QEMU_HEADER_MSB ((uint8_t)(QEMU_HEADER_SIGNATURE >> 8))
/** @brief Least significant byte of @ref QEMU_HEADER_SIGNATURE. */
#define QEMU_HEADER_LSB ((uint8_t)(QEMU_HEADER_SIGNATURE & 0x00FF))

/**
 * @brief Initialize the channel state and allocate its buffers.
 *
 * @param[out] state State to initialize.
 */
void qemu_serial_private_init_state(QemuSerialGlobals *state);

/**
 * @brief Assemble a message from the bytes received by the ISR.
 *
 * Consumes the ISR buffer and advances the receive state machine. A frame not completed within
 * @ref QEMU_RECV_PACKET_TIMEOUT_SEC is dropped.
 *
 * @param[in,out] state Channel state.
 * @param[out] msg_bytes Payload length of the assembled message.
 * @param[out] protocol Protocol of the assembled message.
 * @return Payload of a complete message, or NULL if none is complete yet.
 */
uint8_t *qemu_serial_private_assemble_message(QemuSerialGlobals *state, uint32_t *msg_bytes,
                                              uint16_t *protocol);

/** @} */
