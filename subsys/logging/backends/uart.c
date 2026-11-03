/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include <pbl/drivers/uart.h>
#include <pbl/kernel/syscall.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/backend.h>
#include <pbl/logging/logging.h>
#include <pbl/services/time.h>
#include <pbl/util/string.h>

#include <board/board.h>
#include <console/serial_console.h>
#include <kernel/pebble_tasks.h>
#include <kernel/util/stack_info.h>
#include <logging/logging_private.h>

#define TIMESTAMP_BUFFER_SIZE 40
static void prv_log_timestamp(void) {
  // Enough stack space to use sprintfs?
  uint32_t stack_space = stack_free_bytes();
  if (stack_space < LOGGING_MIN_STACK_FOR_SPRINTF) {
    serial_console_write_log_message(LOGGING_STACK_FULL_MSG);
    serial_console_write_log_message(" ");
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

  serial_console_write_log_message(buffer);
}

static void prv_log_serial(uint8_t log_level, const char *src_filename, int src_line_number,
                           const char *message) {
  if (!serial_console_is_logging_enabled() && log_level != LOG_LEVEL_ALWAYS) {
    return;
  }

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
    serial_console_write_log_message(buffer);
  }

  // Start out with the timestamp
  prv_log_timestamp();

  // Write out the filename
  src_filename = GET_FILE_NAME(src_filename);
  serial_console_write_log_message(src_filename);

  // Write out the line number
  {
    char line_number_buffer[12];
    itoa_int(src_line_number, line_number_buffer, 10);
    serial_console_write_log_message(":");
    serial_console_write_log_message(line_number_buffer);
    serial_console_write_log_message("> ");
  }

  // Write the actual log message.
  serial_console_write_log_message(message);

  // Append our newlines and our trailing null
  serial_console_write_log_message("\r\n");
}

static void prv_backend_process(const struct pbl_log_backend *backend,
                                const struct pbl_log_msg *msg) {
  if (msg->flags & PBL_LOG_MSG_FAULT) {
    serial_console_write_log_message(msg->text);
    serial_console_write_log_message("\r\n");
    return;
  }

  prv_log_serial(msg->level, msg->file, msg->line, msg->text);
}

static void prv_backend_early_init(const struct pbl_log_backend *backend) {
  const struct pbl_uart_config cfg = {.baudrate = CONFIG_CONSOLE_BAUDRATE};

  (void)pbl_uart_early_init(CONSOLE_UART, &cfg);
}

static const struct pbl_log_backend_api s_api = {
  .early_init = prv_backend_early_init,
  .process = prv_backend_process,
};

PBL_LOG_BACKEND_DEFINE(uart, s_api, CONFIG_LOG_BACKEND_UART_LEVEL, true);
