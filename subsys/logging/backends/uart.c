/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include <pbl/console/console.h>
#include <pbl/drivers/uart.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/sched.h>
#include <pbl/kernel/syscall.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/backend.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/interrupts.h>
#include <pbl/services/time.h>
#include <pbl/util/circular_buffer.h>
#include <pbl/util/math.h>
#include <pbl/util/string.h>

#include <board/board.h>
#include <kernel/events.h>
#include <kernel/pebble_tasks.h>
#include <kernel/util/stack_info.h>
#include <logging/logging_private.h>

static void prv_write(const char *str) {
  pbl_console_write_raw(str, strlen(str));
}

#define TIMESTAMP_BUFFER_SIZE 40
static void prv_log_timestamp(void) {
  // Enough stack space to use sprintfs?
  uint32_t stack_space = stack_free_bytes();
  if (stack_space < LOGGING_MIN_STACK_FOR_SPRINTF) {
    prv_write(LOGGING_STACK_FULL_MSG);
    prv_write(" ");
    return;
  }

  char buffer[TIMESTAMP_BUFFER_SIZE];

  uint64_t now_ms = pbl_realtime_ms();
  time_t time_seconds = (time_t)(now_ms / 1000U);
  uint16_t time_ms = (uint16_t)(now_ms % 1000U);
  struct pbl_tm time_seconds_calendar;
  pbl_gmtime_r(&time_seconds, &time_seconds_calendar);

  sniprintf(buffer, TIMESTAMP_BUFFER_SIZE, "%02u:%02u:%02u.%03u ", time_seconds_calendar.tm_hour,
            time_seconds_calendar.tm_min, time_seconds_calendar.tm_sec, time_ms);

  prv_write(buffer);
}

static void prv_log_serial(uint8_t log_level, const char *src_filename, int src_line_number,
                           const char *message) {
  // Log the log level and the current task+privilege level
  {
#ifdef CONFIG_LOG_TASK_PREFIX
    unsigned char task_char = pebble_task_get_char(pebble_task_get_current());
    if (!pbl_is_user_context()) {
      task_char = toupper(task_char);
    }
#else
    unsigned char task_char = '-';
#endif

    char buffer[] = {pbl_log_get_level_char(log_level), ' ', task_char, ' ', 0};
    prv_write(buffer);
  }

  // Start out with the timestamp
  prv_log_timestamp();

  // Write out the filename
  src_filename = GET_FILE_NAME(src_filename);
  prv_write(src_filename);

  // Write out the line number
  {
    char line_number_buffer[12];
    itoa_int(src_line_number, line_number_buffer, 10);
    prv_write(":");
    prv_write(line_number_buffer);
    prv_write("> ");
  }

  // Write the actual log message.
  prv_write(message);

  // Append our newlines and our trailing null
  prv_write("\r\n");
}

static void prv_log_line(uint8_t log_level, const char *src_filename, int src_line_number,
                         const char *message) {
  pbl_console_log_begin();
  prv_log_serial(log_level, src_filename, src_line_number, message);
  pbl_console_log_end();
}

// Messages from where the port can't be locked wait here for KernelMain, as writing them right
// away could land in the middle of another user's output, such as a frame.
#define ISR_MSG_MAX 128U

static CircularBuffer s_isr_buffer;
static uint8_t s_isr_buffer_storage[256];

typedef struct PBL_PACKED IsrMsgHeader {
  uint8_t level;
  // 0 for a dropped message
  uint8_t len;
} IsrMsgHeader;

static bool prv_isr_read(void *buf, size_t len) {
  pbl_irq_lock();
  const bool ok = circular_buffer_copy(&s_isr_buffer, buf, len);
  if (ok) {
    circular_buffer_consume(&s_isr_buffer, len);
  }
  pbl_irq_unlock();

  return ok;
}

static void prv_isr_flush(void *unused) {
  IsrMsgHeader hdr;

  while (prv_isr_read(&hdr, sizeof(hdr))) {
    char text[ISR_MSG_MAX + 1U];

    if (hdr.len == 0U) {
      prv_log_line(LOG_LEVEL_ERROR, "", 0, "ISR message dropped");
      continue;
    }
    prv_isr_read(text, hdr.len);
    text[hdr.len] = '\0';
    prv_log_line(hdr.level, "", 0, text);
  }
}

static void prv_isr_enqueue(uint8_t level, const char *text) {
  const size_t len = MIN(strlen(text), ISR_MSG_MAX);

  pbl_irq_lock();
  const bool was_empty = (circular_buffer_get_read_space_remaining(&s_isr_buffer) == 0U);
  const size_t space = circular_buffer_get_write_space_remaining(&s_isr_buffer);

  if (space >= sizeof(IsrMsgHeader) + len) {
    const IsrMsgHeader hdr = {.level = level, .len = (uint8_t)len};
    circular_buffer_write(&s_isr_buffer, &hdr, sizeof(hdr));
    circular_buffer_write(&s_isr_buffer, text, len);
  } else if (space >= sizeof(IsrMsgHeader)) {
    const IsrMsgHeader hdr = {.level = level, .len = 0U};
    circular_buffer_write(&s_isr_buffer, &hdr, sizeof(hdr));
  }

  if (was_empty) {
    PebbleEvent e = {.type = PEBBLE_CALLBACK_EVENT, .callback = {.callback = prv_isr_flush}};
    event_put_isr(&e);
  }
  pbl_irq_unlock();
}

static void prv_backend_init(const struct pbl_log_backend *backend) {
  circular_buffer_init(&s_isr_buffer, s_isr_buffer_storage, sizeof(s_isr_buffer_storage));
}

static void prv_backend_panic(const struct pbl_log_backend *backend) {
  prv_isr_flush(nullptr);
}

static void prv_backend_process(const struct pbl_log_backend *backend,
                                const struct pbl_log_msg *msg) {
  if (msg->flags & PBL_LOG_MSG_FAULT) {
    prv_write(msg->text);
    prv_write("\r\n");
    return;
  }

  if ((msg->flags & PBL_LOG_MSG_ASYNC) &&
      (pbl_irq_is_locked() || mcu_state_is_isr() || pbl_sched_is_locked())) {
    prv_isr_enqueue(msg->level, msg->text);
    return;
  }

  prv_log_line(msg->level, msg->file, msg->line, msg->text);
}

static void prv_backend_early_init(const struct pbl_log_backend *backend) {
  const struct pbl_uart_config cfg = {.baudrate = CONFIG_CONSOLE_BAUDRATE};

  (void)pbl_uart_early_init(CONSOLE_UART, &cfg);
}

static const struct pbl_log_backend_api s_api = {
  .early_init = prv_backend_early_init,
  .init = prv_backend_init,
  .process = prv_backend_process,
  .panic = prv_backend_panic,
};

PBL_LOG_BACKEND_DEFINE(uart, s_api, CONFIG_LOG_BACKEND_UART_LEVEL, true);
