/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup services_put_bytes Put bytes
 * @ingroup services
 * @brief Transfer of firmware, resources, apps and files from the phone to the watch.
 *
 * A Pebble Protocol endpoint driven by the phone. Every request is answered with an ack or nack
 * carrying the transfer token; multi-byte fields are big-endian. One transfer runs at a time and
 * is cleaned up after 30 s without requests, or when the system session disconnects.
 *
 * @code{.unparsed}
 * phone                                          watch
 * Init(type, total_size, bank index / cookie / filename)
 *                                         ---->  storage set up (flash erased for raw objects)
 *                                         <----  Ack(token)
 * Put(token, length, data)                ---->  data appended to storage
 *                                         <----  Ack(token)
 * ... repeated until total_size bytes are sent ...
 * Commit(token, legacy CRC-32 of the data) ---->  CRC checked, object marked installable
 *                                         <----  Ack(token)
 * Install(token)                          ---->  boot bits set (firmware + system resources,
 *                                                recovery), transfer finished
 *                                         <----  Ack(token)
 * @endcode
 *
 * Abort(token) cancels a transfer. Firmware, recovery and system resources are only accepted
 * during a firmware update, are written raw to flash and can resume an interrupted transfer from
 * an append offset in the Init request (see pb_storage_get_status()). Other objects are written
 * to PFS files.
 * @{
 */

/** @brief Communication session event, see @c PebbleCommSessionEvent. */
typedef struct PebbleCommSessionEvent PebbleCommSessionEvent;

/** @brief Object types. */
typedef enum {
  /** Unknown. */
  ObjectUnknown = 0x00,
  /** Firmware image. */
  ObjectFirmware = 0x01,
  /** Recovery firmware image. */
  ObjectRecovery = 0x02,
  /** System resources. */
  ObjectSysResources = 0x03,
  /** App resources. */
  ObjectAppResources = 0x04,
  /** App binary. */
  ObjectWatchApp = 0x05,
  /** PFS file. */
  ObjectFile = 0x06,
  /** Worker binary. */
  ObjectWatchWorker = 0x07,
  /** Number of object types. */
  NumObjects
} PutBytesObjectType;

/** @brief Progress of a partially written object. */
typedef struct PbInstallStatus {
  /** Number of bytes written. */
  uint32_t num_bytes_written;
  /** Legacy CRC-32 of the bytes written. */
  uint32_t crc_of_bytes;
} PbInstallStatus;

/** @brief Initialize put bytes. */
void put_bytes_init(void);

/**
 * @brief Cancel an ongoing app, app resources or worker transfer.
 *
 * Does nothing when idle or for other object types. The phone's next requests are nacked. Must be
 * called from KernelBG.
 */
void put_bytes_cancel(void);

/** @brief Reset all put bytes state, for unit tests only. */
void put_bytes_deinit(void);

/**
 * @brief Expect an Init request within a timeout.
 *
 * If none arrives in time, a put bytes init timeout event is raised. Does nothing if a transfer
 * is in progress.
 *
 * @param timeout_ms Timeout in milliseconds.
 */
void put_bytes_expect_init(uint32_t timeout_ms);

/**
 * @brief Handle a communication session event.
 *
 * Cancels the ongoing transfer when the system session closes.
 *
 * @param app_event Session event.
 */
void put_bytes_handle_comm_session_event(const PebbleCommSessionEvent *app_event);

/** @} */
