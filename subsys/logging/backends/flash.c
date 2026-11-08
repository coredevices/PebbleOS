/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "flash.h"

#include <string.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sched.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/backend.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/interrupts.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/shared_cbuf.h>

#include <logging/logging_private.h>

static struct pbl_shared_cbuf s_buffer;
static struct pbl_shared_cbuf_client s_buffer_client;
// During normal operation, since log messages are hashed, most are only 30-40
// bytes long with the longest being about 80 bytes, so this is enough for 15-40
// or so messages.
static uint8_t s_buffer_storage[1200];
static PBL_MUTEX_DEFINE(s_buffer_mutex);      //!< Protects s_buffer
static PBL_MUTEX_DEFINE(s_flash_write_mutex); //!< Protects log line consistency
static bool
    s_is_flash_write_scheduled; //!< true if handle_buffer_sync SysWorkq callback is scheduled

static void write_message(void) {
  // Note that we should enter this function with the buffer mutex held.

  const uint8_t *data_read;
  uint16_t read_length;

  // Read the header part
  bool result =
      pbl_shared_cbuf_read(&s_buffer, &s_buffer_client, sizeof(uint8_t), &data_read, &read_length);
  PBL_ASSERTN(result);
  PBL_ASSERT(read_length == sizeof(uint8_t), "read_length %u sizeof(uint8_t) %zu", read_length,
             sizeof(uint8_t));
  uint8_t msg_length = *data_read;

  if (pbl_shared_cbuf_get_read_space_remaining(&s_buffer, &s_buffer_client) <
      msg_length + sizeof(uint8_t)) {
    return; // Not ready yet, consume nothing.
  }

  // Flash_logging_log_start can trigger a flash erase. Release the buffer mutex
  // to allow logging while the (slow) erase completes.
  pbl_mutex_unlock(&s_buffer_mutex);
  uint32_t flash_addr = pbl_log__flash_storage_log_start(msg_length);
  pbl_mutex_lock(&s_buffer_mutex, PBL_FOREVER);
  if (flash_addr == PBL_LOG__FLASH_INVALID_ADDR) {
    return;
  }

  pbl_shared_cbuf_consume(&s_buffer, &s_buffer_client, read_length);

  while (msg_length > 0) {
    pbl_mutex_unlock(&s_buffer_mutex);

    // Note that this buffer read really should be done with the buffer mutex held.
    // This works only because writes to the buffer do not advance slackers.
    result =
        pbl_shared_cbuf_read(&s_buffer, &s_buffer_client, msg_length, &data_read, &read_length);
    PBL_ASSERTN(result);
    msg_length -= read_length;

    pbl_log__flash_storage_write(data_read, flash_addr, read_length);
    flash_addr += read_length;

    pbl_mutex_lock(&s_buffer_mutex, PBL_FOREVER);
    pbl_shared_cbuf_consume(&s_buffer, &s_buffer_client, read_length);
  }

  // We should still be holding the buffer lock here...
}

static void handle_buffer_sync(void *data) {
  const bool is_async = (uintptr_t)data;

  pbl_mutex_lock(&s_flash_write_mutex, PBL_FOREVER);
  pbl_mutex_lock(&s_buffer_mutex, PBL_FOREVER);

  // Bound the drain to what was already pending when this callback ran.  Each
  // write_message() releases the buffer mutex around the flash write so other
  // tasks (BT, KernelMain) can keep producing log lines; if we kept the outer
  // while running on the live "remaining" count, a chatty producer can keep
  // this callback executing on SysWorkq forever.  That starves every
  // other system-task callback (PutBytes processing in particular) and the
  // emulator wedges with no progress and no watchdog reboot.  Drain the
  // initial backlog and leave any newly-arrived bytes for the next callback;
  // the s_is_flash_write_scheduled flag below ensures producers re-arm us.
  size_t budget = pbl_shared_cbuf_get_read_space_remaining(&s_buffer, &s_buffer_client);
  while (budget > 0 && pbl_shared_cbuf_get_read_space_remaining(&s_buffer, &s_buffer_client) > 0) {
    size_t before = pbl_shared_cbuf_get_read_space_remaining(&s_buffer, &s_buffer_client);
    write_message();
    // The above function mucks with the mutex
    pbl_mutex_assert_held(&s_buffer_mutex, true /* is_held */);
    size_t after = pbl_shared_cbuf_get_read_space_remaining(&s_buffer, &s_buffer_client);
    if (after >= before) {
      // write_message() didn't make progress (e.g. partial message not ready);
      // bail rather than spinning.
      break;
    }
    size_t consumed = before - after;
    budget = (consumed >= budget) ? 0 : budget - consumed;
  }

  if (is_async) {
    s_is_flash_write_scheduled = false;
  }

  pbl_mutex_unlock(&s_buffer_mutex);
  pbl_mutex_unlock(&s_flash_write_mutex);
}

void pbl_log_flash_init(void) {
  pbl_log__flash_storage_init();

  pbl_shared_cbuf_init(&s_buffer, s_buffer_storage, sizeof(s_buffer_storage));
  pbl_shared_cbuf_add_client(&s_buffer, &s_buffer_client);

  pbl_log_backend_start(PBL_LOG_BACKEND_GET(flash));
}

// Return true on success
static bool write_buffer_locking(const LogBinaryMessage *header, const char *text, bool async) {
  const int length = sizeof(*header) + header->message_length;
  bool success = false;

  do {
    pbl_mutex_lock(&s_buffer_mutex, PBL_FOREVER);
    if (pbl_shared_cbuf_get_write_space_remaining(&s_buffer) >= length + 1) {
      // Ideally we could figure out a way to skip out on this copy but then you'd potentially need
      // to sniprintf into a non-contiguous buffer... whatever, we have CPU to burn.
      uint8_t msg_length = length;

      // Do not advance slackers. Data loss and/or corruption will occur! See write_message()
      pbl_shared_cbuf_write(&s_buffer, &msg_length, sizeof(uint8_t), false /*advance_slackers*/);
      pbl_shared_cbuf_write(&s_buffer, (const uint8_t *)header, sizeof(*header),
                            false /*advance_slackers*/);
      pbl_shared_cbuf_write(&s_buffer, (const uint8_t *)text, header->message_length,
                            false /*advance_slackers*/);

      success = true;
    }
    pbl_mutex_unlock(&s_buffer_mutex);

    // If we failed to buffer this message, flush the buffer to cache to make room.
    // Otherwise, if this is a sync message, flush this message to flash.
    if (!success || !async) {
      handle_buffer_sync((void *)(uintptr_t)false /* !is_async */);
    }
  } while (!success); // Loop until the buffer copy succeeds. If sync, also wait until this message
                      // is written to flash.
                      // It's highly unlikely that another task will win the race and completely
                      // fill the buffer between the flash write and the next buffer write attempt.
                      // If so, there are bigger issues.

  if (async) {
    pbl_mutex_lock(&s_buffer_mutex, PBL_FOREVER);
    if (!s_is_flash_write_scheduled) {
      s_is_flash_write_scheduled = true;
      pbl_sys_work_call(handle_buffer_sync, (void *)(uintptr_t)true /* is_async */);
    }
    pbl_mutex_unlock(&s_buffer_mutex);
  }

  return success;
}

static void prv_backend_process(const struct pbl_log_backend *backend,
                                const struct pbl_log_msg *msg) {
  // App messages are not stored, and storing needs the scheduler.
  if (msg->app_uuid || (msg->flags & PBL_LOG_MSG_FAULT) || pbl_irq_is_locked() ||
      mcu_state_is_isr() || pbl_sched_is_locked()) {
    return;
  }

  LogBinaryMessage header = {
    .timestamp = pbl_cpu_to_be32(msg->timestamp),
    .log_level = msg->level,
    .message_length = msg->text_len,
    .line_number = pbl_cpu_to_be16(msg->line),
  };
  strncpy(header.filename, msg->file, sizeof(header.filename));

  write_buffer_locking(&header, msg->text, msg->flags & PBL_LOG_MSG_ASYNC);
}

static const struct pbl_log_backend_api s_api = {
  .process = prv_backend_process,
  .replay = true,
};

PBL_LOG_BACKEND_DEFINE(flash, s_api, CONFIG_LOG_BACKEND_FLASH_LEVEL, false);
