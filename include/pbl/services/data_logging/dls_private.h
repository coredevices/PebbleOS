/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/data_logging.h"
#include <pbl/drivers/rtc.h>
#include "flash_region/flash_region.h"
#include "kernel/pebble_tasks.h"
#include "pbl/kernel/mutex.h"
#include "pbl/services/comm_session/protocol.h"
#include "system/hexdump.h"
#include "pbl/kernel/compiler.h"
#include "pbl/util/shared_cbuf.h"
#include "pbl/util/units.h"
#include "pbl/util/uuid.h"

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/**
 * @defgroup services_data_logging_dls_private Data logging internals
 * @ingroup services_data_logging
 * @brief Session structures, limits and wire format shared by the data logging service.
 * @{
 */

/** @brief Prefix of session file names, followed by the decimal session ID. */
#define DLS_FILE_NAME_PREFIX "dls_storage_"
/** @brief Size of a buffer holding a session file name. */
static const uint32_t DLS_FILE_NAME_MAX_LEN = 20;
/** @brief Initial size of a session file. */
static const uint32_t DLS_FILE_INIT_SIZE_BYTES = PBL_KIB(4);

/**
 * @brief Minimum free space added when a session file grows.
 *
 * A file grows by half its unread data, clamped to this and @ref DLS_MAX_FILE_FREE_BYTES.
 */
static const uint32_t DLS_MIN_FILE_FREE_BYTES = PBL_KIB(8);
/** @brief Maximum free space added when a session file grows. */
static const uint32_t DLS_MAX_FILE_FREE_BYTES = PBL_KIB(100);

/** @brief Free space left at the end of a session file below which the file is grown. */
static const uint32_t DLS_MIN_FREE_BYTES = PBL_KIB(1);

/** @brief Maximum number of sessions. */
static const uint32_t DLS_MAX_NUM_SESSIONS = 20;

/** @brief Maximum file system space used by all session files. */
static const uint32_t DLS_TOTAL_STORAGE_BYTES = PBL_KIB(640);

/** @brief Space available to session files beyond their initial size. */
#define DLS_MAX_DATA_BYTES \
  (DLS_TOTAL_STORAGE_BYTES - (DLS_MAX_NUM_SESSIONS * DLS_FILE_INIT_SIZE_BYTES))

/** @brief Session status. */
typedef enum {
  /** Created and still being logged to. */
  DataLoggingStatusActive = 0x01,
  /** Closed by its owner, or its owner exited; remaining data is still sent to the phone. */
  DataLoggingStatusInactive = 0x02,
} DataLoggingStatus;

/** @brief Data logging endpoint commands. */
typedef enum {
  /** Watch opens a session. */
  DataLoggingEndpointCmdOpen = 0x01,
  /** Watch sends session data, see @ref DataLoggingSendDataMessage. */
  DataLoggingEndpointCmdData = 0x02,
  /** Watch closes a session. */
  DataLoggingEndpointCmdClose = 0x03,
  /** Phone reports the sessions it knows about. */
  DataLoggingEndpointCmdReport = 0x04,
  /** Phone acknowledges an open or data message. */
  DataLoggingEndpointCmdAck = 0x05,
  /** Phone rejects an open or data message. */
  DataLoggingEndpointCmdNack = 0x06,
  /** Watch reports that an ack was not received in time. */
  DataLoggingEndpointCmdTimeout = 0x07,
  /** Phone asks to send a session's data now. */
  DataLoggingEndpointCmdEmptySession = 0x08,
  /** Phone asks whether sending is enabled. */
  DataLoggingEndpointCmdGetSendEnableReq = 0x09,
  /** Watch answers @ref DataLoggingEndpointCmdGetSendEnableReq. */
  DataLoggingEndpointCmdGetSendEnableRsp = 0x0A,
  /** Phone enables or disables sending. */
  DataLoggingEndpointCmdSetSendEnable = 0x0B,
} DataLoggingEndpointCmd;

/**
 * @brief Mask of the command in the first byte of an endpoint message.
 *
 * The top bit is set in commands from the phone and clear in commands from the watch.
 */
static const uint8_t DLS_ENDPOINT_CMD_MASK = 0x7f;

/** @brief Value of @ref DataLoggingSessionStorage::fd when the session has no open file. */
#define DLS_INVALID_FILE (-1)
/** @brief Location of a session's data in its file. */
typedef struct DataLoggingSessionStorage {
  /** PFS file descriptor, or @ref DLS_INVALID_FILE when not open. */
  int fd;

  /** File offset of the next write. */
  uint32_t write_offset;

  /** File offset of the next read. */
  uint32_t read_offset;

  /** Number of unread bytes. */
  uint32_t num_bytes;
} DataLoggingSessionStorage;

/**
 * @brief Endpoint state of a session.
 *
 * @verbatim
    +----------+  Rx Ack    +----------+    Tx Data   +----------+
    | Opening  |----------->| Idle     |+------------>| Sending  |
    +----------+            +----------+              +----------+
                                 ^                         |
                                 |       Rx Ack            |
                                 +-------------------------+
   @endverbatim
 */
typedef enum {
  /** Waiting for the phone to ack the open message. */
  DataLoggingSessionCommStateOpening,
  /** Ready to send data. */
  DataLoggingSessionCommStateIdle,
  /** Waiting for the phone to ack sent data. */
  DataLoggingSessionCommStateSending,
} DataLoggingSessionCommState;

/** @brief Endpoint state of a session. */
typedef struct {
  /** Session ID, chosen by the watch and unique among its sessions. */
  uint8_t session_id;

  /** Endpoint state. */
  DataLoggingSessionCommState state : 8;

  /** Number of times the phone nacked this session. */
  uint8_t nack_count;

  /** Bytes sent to the phone and not acked yet. */
  int num_bytes_pending;

  /** Time in RTC ticks at which the pending ack times out, 0 when not waiting for one. */
  RtcTicks ack_timeout;
} DataLoggingSessionComm;

/** @brief State of an active session. */
typedef struct {
  /** Session lock, see dls_lock_session(). */
  struct pbl_mutex mutex;
  /** Circular buffer of a buffered session. */
  struct pbl_shared_cbuf buffer;
  /** Reader of @ref buffer, consumed by KernelBG. */
  struct pbl_shared_cbuf_client buffer_client;
  /** Storage of @ref buffer, NULL for unbuffered sessions. */
  uint8_t *buffer_storage;
  /** @ref buffer_storage is on the kernel heap, else on the heap of the dls_create() caller. */
  bool buffer_in_kernel_heap : 1;
  /** A flash write has been requested from KernelBG and not run yet. */
  bool write_request_pending : 1;
  /** Inactivate the session once its last lock is released, see dls_unlock_session(). */
  bool inactivate_pending : 1;
  /** Number of locks held, changed under the list mutex. The state is freed only at 0. */
  uint8_t open_count;
} DataLoggingActiveState;

/** @brief Data logging session. */
typedef struct DataLoggingSession {
  // FIXME use a ListNode instead of this custom list
  /** Next session in the list. */
  struct DataLoggingSession *next;

  /** Owner UUID. */
  Uuid app_uuid;
  /** Session tag. */
  uint32_t tag;
  /** Task that created the session. */
  PebbleTask task;

  /** Item type. */
  DataLoggingItemType item_type : 4;
  /** Session status. */
  DataLoggingStatus status : 4;
  /** Item size in bytes. */
  uint16_t item_size;

  /** Creation time. */
  time_t session_created_timestamp;

  /** Endpoint state. */
  DataLoggingSessionComm comm;

  /** Flash storage state. */
  DataLoggingSessionStorage storage;

  /** Active state, NULL for inactive sessions. */
  DataLoggingActiveState *data;
} DataLoggingSession;

/**
 * @brief Send the next chunk of a session's stored data to the phone.
 *
 * Must be called from KernelBG. Removes inactive sessions with no data left. Active sessions are
 * only sent when they hold enough data, unless @p empty is set.
 *
 * @param logging_session Session.
 * @param empty Send even if little data is stored.
 * @return false on unexpected errors, true otherwise (including when nothing was sent).
 */
bool dls_private_send_session(DataLoggingSession *logging_session, bool empty);

/**
 * @brief Reset the endpoint state of all sessions after a disconnection.
 *
 * Must be called from KernelBG.
 *
 * @param data Unused.
 */
void dls_private_handle_disconnect(void *data);

/**
 * @brief Not implemented, use dls_get_send_enable().
 *
 * @return Send enable setting.
 */
bool dls_private_get_send_enable(void);
/**
 * @brief Not implemented, use dls_set_send_enable_pp().
 *
 * @param setting Send enable setting.
 */
void dls_private_set_send_enable(bool setting);

/** @brief Data message, sent with @ref DataLoggingEndpointCmdData. */
typedef struct PBL_PACKED {
  /** @ref DataLoggingEndpointCmdData. */
  uint8_t command;
  /** Session ID. */
  uint8_t session_id;
  /** Items left after this message; currently always 0xffff. */
  uint32_t items_left_hereafter;
  /** Legacy CRC-32 of @ref bytes. */
  uint32_t crc32;
  /** Whole items. */
  uint8_t bytes[];
} DataLoggingSendDataMessage;

/** @brief Largest item size, and largest dls_log() write, for buffered sessions. */
static const uint32_t DLS_SESSION_MAX_BUFFERED_ITEM_SIZE = 300;

/**
 * @brief Size of a buffered session's buffer.
 *
 * One byte more than @ref DLS_SESSION_MAX_BUFFERED_ITEM_SIZE, as needed by the circular buffer.
 */
#define DLS_SESSION_MIN_BUFFER_SIZE (DLS_SESSION_MAX_BUFFERED_ITEM_SIZE + 1)

/** @brief Largest data message payload, and largest item size for unbuffered sessions. */
static const uint32_t DLS_ENDPOINT_MAX_PAYLOAD =
    (COMM_MAX_OUTBOUND_PAYLOAD_SIZE - sizeof(DataLoggingSendDataMessage));

/**
 * @brief Read session data, for unit tests only.
 *
 * @param logging_session Session.
 * @param[out] buffer Destination.
 * @param num_bytes Maximum number of bytes to read.
 * @return See dls_storage_read().
 */
int dls_test_read(DataLoggingSession *logging_session, uint8_t *buffer, int num_bytes);

/**
 * @brief Consume session data, for unit tests only.
 *
 * @param logging_session Session.
 * @param num_bytes Number of bytes to consume.
 * @return @p num_bytes.
 */
int dls_test_consume(DataLoggingSession *logging_session, int num_bytes);

/**
 * @brief Get the number of unread bytes, for unit tests only.
 *
 * @param logging_session Session.
 * @return Unread bytes.
 */
int dls_test_get_num_bytes(DataLoggingSession *logging_session);

/**
 * @brief Get the session tag, for unit tests only.
 *
 * @param logging_session Session.
 * @return Session tag.
 */
int dls_test_get_tag(DataLoggingSession *logging_session);

/**
 * @brief Get the session ID, for unit tests only.
 *
 * @param logging_session Session.
 * @return Session ID.
 */
uint8_t dls_test_get_session_id(DataLoggingSession *logging_session);

/** @} */
