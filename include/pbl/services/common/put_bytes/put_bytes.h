/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct PebbleCommSessionEvent PebbleCommSessionEvent;

typedef enum {
  ObjectUnknown = 0x00,
  ObjectFirmware = 0x01,
  ObjectRecovery = 0x02,
  ObjectSysResources = 0x03,
  ObjectAppResources = 0x04,
  ObjectWatchApp = 0x05,
  ObjectFile = 0x06,
  ObjectWatchWorker = 0x07,
  NumObjects
} PutBytesObjectType;

typedef struct PbInstallStatus {
  uint32_t num_bytes_written;
  uint32_t crc_of_bytes;
} PbInstallStatus;

void put_bytes_init(void);

//! Tells put_bytes to clean up instantly. If put_bytes is already cleaned up, this is a no-op.
//! Any future messages sent by clients will be NACK'ed appropriately.
//! NOTE: Must be called from the KernelBackground task.
void put_bytes_cancel(void);

//! Reset all put bytes state. Only useful for unit tests.
void put_bytes_deinit(void);

//! Sets an initialization timeout for put_bytes.
//! If the phone doesn't send any data within the specified timeout,
//! put_bytes raises a timeout event.
void put_bytes_expect_init(uint32_t timeout_ms);

//! Informs Put Bytes when the Pebble app disconnects to the Pebble, to make
//! it cancel any on-going transaction.
void put_bytes_handle_comm_session_event(const PebbleCommSessionEvent *app_event);
