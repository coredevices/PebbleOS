/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include <pbl/drivers/rtc.h>
#include <pbl/shell/shell.h>

#include "debug/flash_logging.h"
#include "logging/logging_private.h"
#include "pbl/services/system_task.h"
#include "util/net.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

static const struct pbl_shell *s_dump_sh;

static bool prv_dump_line_cb(uint8_t *msg, uint32_t total_length) {
  LogBinaryMessage *message = (LogBinaryMessage *)msg;
  char time_buffer[TIME_STRING_BUFFER_SIZE];

  if (s_dump_sh == NULL) {
    return false;
  }

  message->message[message->message_length] = 0;
  pbl_shell_print(s_dump_sh, "%c %s %s:%d> %s", pbl_log_get_level_char(message->log_level),
                  time_t_to_string(time_buffer, htonl(message->timestamp)), message->filename,
                  (int)htons(message->line_number), message->message);
  return true;
}

static void prv_dump_completed_cb(bool success) {
  const struct pbl_shell *sh = s_dump_sh;

  if (sh == NULL) {
    return;
  }

  s_dump_sh = NULL;
  pbl_shell_cmd_done(sh, success ? 0 : -EIO);
}

static int prv_dump(const struct pbl_shell *sh, int generation) {
  // Completion runs later on this same task, or synchronously (with s_dump_sh still unset) when
  // the generation doesn't exist.
  if (!flash_dump_log_file(generation, prv_dump_line_cb, prv_dump_completed_cb)) {
    pbl_shell_error(sh, "log generation %d not found", generation);
    return -ENOENT;
  }

  s_dump_sh = sh;
  return -EINPROGRESS;
}

static int prv_cmd_dump_current(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_dump(sh, 0);
}

static int prv_cmd_dump_last(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_dump(sh, 1);
}

static int prv_cmd_dump_gen(const struct pbl_shell *sh, size_t argc, char **argv) {
  long generation;

  if (pbl_shell_strtol(argv[1], &generation) != 0 || generation < 0) {
    pbl_shell_error(sh, "invalid generation '%s'", argv[1]);
    return -EINVAL;
  }

  return prv_dump(sh, generation);
}

static void prv_spam_cb(void *data) {
  uint32_t iteration = (uintptr_t)data;
  uint8_t buffer[128];
  time_t base = rtc_get_time();

  for (int i = 0; i < 16; ++i) {
    LogBinaryMessage *msg = (LogBinaryMessage *)buffer;
    msg->timestamp = htonl(base + iteration * 16 + i);
    msg->log_level = LOG_LEVEL_ERROR;
    msg->message_length = sizeof(buffer) - sizeof(LogBinaryMessage);
    msg->line_number = 0;
    strncpy(msg->filename, "spam.exe", sizeof(msg->filename));
    char letter = 'A' + i;
    memset(msg->message, letter, msg->message_length - 1);
    msg->message[msg->message_length - 1] = 0;

    uint32_t flash_addr = flash_logging_log_start(sizeof(buffer));
    flash_logging_write(buffer, flash_addr, sizeof(buffer));
  }
}

static int prv_cmd_spam(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_shell_print(sh, "spam logs!");
  for (int i = 0; i < 16; ++i) {
    system_task_add_callback(prv_spam_cb, (void *)(uintptr_t)i);
  }
  return 0;
}

static const struct pbl_shell_cmd sub_log_dump[] = {
  PBL_SHELL_CMD(current, NULL, "Dump the current boot log", prv_cmd_dump_current),
  PBL_SHELL_CMD(last, NULL, "Dump the previous boot log", prv_cmd_dump_last),
  PBL_SHELL_CMD_ARG(gen, NULL, "Dump a boot log generation <n>", prv_cmd_dump_gen, 2, 0),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_SUBCMD_ADD(sub_log, dump, sub_log_dump, "Dump flash logs", NULL, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_log, spam, NULL, "Fill the flash log with junk", prv_cmd_spam, 0, 0);

#endif
