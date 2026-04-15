/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Logging Memfault chunks to datalogging can only happen in normal FW
#include "memfault/components.h"

#include "pbl/services/common/system_task.h"
#include "pbl/services/normal/data_logging/data_logging_service.h"
#include "pbl/services/common/new_timer/new_timer.h"
#include "system/logging.h"

#define MAX_CHUNK_SIZE 250
#define MEMFAULT_CHUNK_COLLECTION_INTERVAL_SECS (15 * 60)

static DataLoggingSessionRef s_chunks_session;
static TimerID s_memfault_chunks_timer;
static TimerID s_memfault_collect_soon_timer;

// Datalogging packet sizes are fixed, so we need a wrapper to include the (variable) chunk size.
typedef struct PACKED {
  uint32_t length;
  uint8_t buf[MAX_CHUNK_SIZE];
} ChunkWrapper;

static void prv_create_dls_session() {
  if (s_chunks_session != NULL) {
    return;
  }
  Uuid system_uuid = UUID_SYSTEM;
  uint32_t item_length = sizeof(ChunkWrapper);
  s_chunks_session = dls_create(
      DlsSystemTagMemfaultChunksSession, DATA_LOGGING_BYTE_ARRAY, item_length, false, false, &system_uuid);
}

static void prv_memfault_gather_chunks() {
  if (!dls_initialized()) {
    // We need to wait until data logging is initialized before we can add chunks
    PBL_LOG_ERR("!dls_initialized");
    return;
  }

  // We can't do this in init_memfault_chunk_collection because datalogging isn't initialized
  // yet, so do it here.
  prv_create_dls_session();
  if (s_chunks_session == NULL) {
    PBL_LOG_ERR("!s_chunks_session");
    return;
  }

  // Check for a pending coredump before gathering, since packetizing consumes it.
  bool had_coredump = memfault_coredump_has_valid_coredump(NULL);

  ChunkWrapper wrapper;
  bool data_available = true;
  size_t buf_len;

  while (data_available) {
    // always reset buf_len to the size of the output buffer before calling
    // memfault_packetizer_get_chunk
    buf_len = MAX_CHUNK_SIZE;
    data_available = memfault_packetizer_get_chunk(wrapper.buf, &buf_len);
    wrapper.length = buf_len;

    if (data_available) {
      bool res = dls_log(s_chunks_session, &wrapper, 1);
      if (res != DATA_LOGGING_SUCCESS) {
        PBL_LOG_ERR("Memfault chunk dls_log failed: %d", res);
        break;
      }
    }
  }

  if (had_coredump) {
    dls_send_all_sessions();
  }
}

static void prv_memfault_gather_chunks_cb() {
    system_task_add_callback(prv_memfault_gather_chunks, NULL);
}

void memfault_chunk_collect(void) {
  prv_memfault_gather_chunks_cb();
}

// Delay before collecting chunks after phone connects, giving the phone time
// to complete the datalogging Report handshake.
#define MEMFAULT_COLLECT_SOON_DELAY_MS (10 * 1000)

static void prv_memfault_collect_soon_cb(void *data) {
  if (memfault_packetizer_data_available()) {
    prv_memfault_gather_chunks_cb();
  }
}

void memfault_chunk_collect_after_delay(void) {
  if (!s_memfault_collect_soon_timer) {
    return;
  }
  new_timer_start(s_memfault_collect_soon_timer, MEMFAULT_COLLECT_SOON_DELAY_MS,
                  prv_memfault_collect_soon_cb, NULL, 0 /*flags*/);
}

void init_memfault_chunk_collection() {
    s_memfault_chunks_timer = new_timer_create();
    s_memfault_collect_soon_timer = new_timer_create();
    new_timer_start(s_memfault_chunks_timer, MEMFAULT_CHUNK_COLLECTION_INTERVAL_SECS * 1000,
        prv_memfault_gather_chunks_cb, (void *) NULL, TIMER_START_FLAG_REPEATING);
}
