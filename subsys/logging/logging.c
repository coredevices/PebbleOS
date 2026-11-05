/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "logging_private.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/irq.h>
#include <pbl/kernel/syscall.h>
#include <pbl/logging/backend.h>
#include <pbl/logging/logging.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/string.h>

#ifndef PBL_NO_LINKER_SCRIPT
extern const struct pbl_log_backend __pbl_log_backends_start[];
extern const struct pbl_log_backend __pbl_log_backends_end[];
#else
extern const struct pbl_log_backend __pbl_log_backends_start[] PBL_UNSORTED_SECTION_START(
    pbl_log_be);
extern const struct pbl_log_backend __pbl_log_backends_end[] PBL_UNSORTED_SECTION_END(pbl_log_be);
#endif

#define PRV_FOREACH_BACKEND(_backend)                                     \
  for (const struct pbl_log_backend *_backend = __pbl_log_backends_start; \
       _backend < __pbl_log_backends_end; _backend++)

bool g_pbl_log_enabled = true;

char pbl_log_get_level_char(const uint8_t log_level) {
  switch (log_level) {
    case LOG_LEVEL_ALWAYS:
      return '*';
    case LOG_LEVEL_ERROR:
      return 'E';
    case LOG_LEVEL_WARNING:
      return 'W';
    case LOG_LEVEL_INFO:
      return 'I';
    case LOG_LEVEL_DEBUG:
      return 'D';
    case LOG_LEVEL_DEBUG_VERBOSE:
      return 'V';
    default:
      return '?';
  }
}

void pbl_log_backend_enable(const struct pbl_log_backend *backend) {
  atomic_store_explicit(&backend->state->enabled, true, memory_order_relaxed);
}

void pbl_log_backend_disable(const struct pbl_log_backend *backend) {
  atomic_store_explicit(&backend->state->enabled, false, memory_order_relaxed);
}

bool pbl_log_backend_is_enabled(const struct pbl_log_backend *backend) {
  return atomic_load_explicit(&backend->state->enabled, memory_order_relaxed);
}

void pbl_log_backend_set_level(const struct pbl_log_backend *backend, int level) {
  atomic_store_explicit(&backend->state->level, (int16_t)level, memory_order_relaxed);
}

const struct pbl_log_backend *pbl_log_backend_get_by_name(const char *name) {
  PRV_FOREACH_BACKEND(backend) {
    if (strcmp(backend->name, name) == 0) {
      return backend;
    }
  }

  return nullptr;
}

void pbl_log_early_init(void) {
  PRV_FOREACH_BACKEND(backend) {
    if (backend->api->early_init) {
      backend->api->early_init(backend);
    }
  }
}

void pbl_log_init(void) {
  PRV_FOREACH_BACKEND(backend) {
    if (backend->api->init) {
      backend->api->init(backend);
    }
  }
}

void pbl_log_panic(void) {
  PRV_FOREACH_BACKEND(backend) {
    if (backend->api->panic &&
        atomic_load_explicit(&backend->state->enabled, memory_order_relaxed)) {
      backend->api->panic(backend);
    }
  }
}

static bool prv_accepts(const struct pbl_log_backend *backend, const struct pbl_log_msg *msg,
                        bool filtered) {
  return filtered || msg->level == LOG_LEVEL_ALWAYS ||
         msg->level <= atomic_load_explicit(&backend->state->level, memory_order_relaxed);
}

#if CONFIG_LOG_BOOT_BUFFER_SIZE > 0
struct prv_boot_rec {
  uint32_t timestamp;
  uint16_t size;
  uint16_t line;
  uint8_t level;
  uint8_t flags;
  uint8_t text_len;
  bool filtered;
  char file[16];
  char text[];
};

static uint8_t s_boot_buf[CONFIG_LOG_BOOT_BUFFER_SIZE] PBL_ALIGNED(4);
static size_t s_boot_len;
static bool s_boot_full;

static void prv_boot_record(const struct pbl_log_msg *msg, bool filtered) {
  const size_t size = (sizeof(struct prv_boot_rec) + msg->text_len + 1U + 3U) & ~3U;

  pbl_irq_lock();
  if (!s_boot_full && s_boot_len + size > sizeof(s_boot_buf)) {
    s_boot_full = true;
  }
  if (!s_boot_full) {
    struct prv_boot_rec *rec = (struct prv_boot_rec *)&s_boot_buf[s_boot_len];
    *rec = (struct prv_boot_rec){
      .timestamp = msg->timestamp,
      .size = (uint16_t)size,
      .line = msg->line,
      .level = msg->level,
      .flags = msg->flags,
      .text_len = msg->text_len,
      .filtered = filtered,
    };
    strncpy(rec->file, msg->file, sizeof(rec->file) - 1U);
    memcpy(rec->text, msg->text, msg->text_len);
    rec->text[msg->text_len] = '\0';
    s_boot_len += size;
  }
  pbl_irq_unlock();
}

static size_t prv_boot_replay(const struct pbl_log_backend *backend, size_t pos, size_t end) {
  while (pos < end) {
    const struct prv_boot_rec *rec = (const struct prv_boot_rec *)&s_boot_buf[pos];
    const struct pbl_log_msg msg = {
      .file = rec->file,
      .text = rec->text,
      .timestamp = rec->timestamp,
      .line = rec->line,
      .level = rec->level,
      .text_len = rec->text_len,
      .flags = rec->flags,
    };

    if (prv_accepts(backend, &msg, rec->filtered)) {
      backend->api->process(backend, &msg);
    }
    pos += rec->size;
  }

  return pos;
}
#endif

void pbl_log_backend_start(const struct pbl_log_backend *backend) {
#if CONFIG_LOG_BOOT_BUFFER_SIZE > 0
  size_t pos = 0;

  if (!backend->api->replay) {
    pbl_log_backend_enable(backend);
    return;
  }

  while (true) {
    pbl_irq_lock();
    const size_t end = s_boot_len;
    if (pos == end) {
      atomic_store_explicit(&backend->state->enabled, true, memory_order_relaxed);
      pbl_irq_unlock();
      return;
    }
    pbl_irq_unlock();

    pos = prv_boot_replay(backend, pos, end);
  }
#else
  pbl_log_backend_enable(backend);
#endif
}

//! @param filtered The level was already checked against a runtime module level.
static void prv_dispatch(const struct pbl_log_msg *msg, bool filtered) {
  bool record = false;

  PRV_FOREACH_BACKEND(backend) {
    if (!prv_accepts(backend, msg, filtered)) {
      continue;
    }

    if (!atomic_load_explicit(&backend->state->enabled, memory_order_relaxed)) {
      record |= backend->api->replay;
      continue;
    }

    backend->api->process(backend, msg);
  }

#if CONFIG_LOG_BOOT_BUFFER_SIZE > 0
  if (record && msg->app_uuid == nullptr && !(msg->flags & PBL_LOG_MSG_FAULT)) {
    prv_boot_record(msg, filtered);
  }
#else
  (void)record;
#endif
}

static void prv_log_binary(LogBinaryMessage *log_message, uint32_t flags, const uint8_t *app_uuid) {
  if (!g_pbl_log_enabled) {
    return;
  }

  const struct pbl_log_msg msg = {
    .file = log_message->filename,
    .text = log_message->message,
    .app_uuid = app_uuid,
    .timestamp = pbl_cpu_to_be32(log_message->timestamp),
    .line = pbl_cpu_to_be16(log_message->line_number),
    .level = log_message->log_level,
    .text_len = log_message->message_length,
    .flags = (flags & PBL_LOG_FLAG_ASYNC) ? PBL_LOG_MSG_ASYNC : 0,
  };

  prv_dispatch(&msg, flags & PBL_LOG_FLAG_FILTERED);
}

void kernel_pbl_log_verify_user(const LogBinaryMessage *log_message) {
  pbl_syscall_verify_user(log_message, sizeof(*log_message));

  const size_t message_length = log_message->message_length;
  pbl_syscall_verify_user(log_message, sizeof(*log_message) + message_length);

  if (strnlen(log_message->filename, sizeof(log_message->filename)) ==
      sizeof(log_message->filename)) {
    pbl_syscall_fail();
  }

  if (strnlen(log_message->message, message_length) == message_length) {
    pbl_syscall_verify_user(log_message, sizeof(*log_message) + message_length + 1);
    if (log_message->message[message_length] != '\0') {
      pbl_syscall_fail();
    }
  }
}

void kernel_pbl_log(LogBinaryMessage *log_message, uint32_t flags) {
  prv_log_binary(log_message, flags, nullptr);
}

void kernel_pbl_log_app(LogBinaryMessage *log_message, const uint8_t *app_uuid) {
  prv_log_binary(log_message, 0, app_uuid);
}

void kernel_pbl_log_from_fault_handler(const char *src_filename, uint16_t src_line_number,
                                       const char *message) {
  const struct pbl_log_msg msg = {
    .file = src_filename,
    .text = message,
    .line = src_line_number,
    .level = LOG_LEVEL_ALWAYS,
    .text_len = strnlen(message, UINT8_MAX),
    .flags = PBL_LOG_MSG_FAULT,
  };

  prv_dispatch(&msg, true);
}

void kernel_pbl_log_from_fault_handler_fmt(const char *src_filename, uint16_t src_line_number,
                                           char *buffer, unsigned int buffer_size, const char *fmt,
                                           ...) {
  va_list ap;
  va_start(ap, fmt);
  vsniprintf(buffer, buffer_size, fmt, ap);
  va_end(ap);

  kernel_pbl_log_from_fault_handler(src_filename, src_line_number, buffer);
}

#ifdef CONFIG_SHELL
#include <errno.h>

#include <pbl/shell/shell.h>

static int prv_cmd_backends(const struct pbl_shell *sh, size_t argc, char **argv) {
  PRV_FOREACH_BACKEND(backend) {
    pbl_shell_print(sh, "%-8s %-8s level %i", backend->name,
                    backend->state->enabled ? "enabled" : "disabled", backend->state->level);
  }

  return 0;
}

static int prv_cmd_level(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_log_backend *backend = pbl_log_backend_get_by_name(argv[1]);
  if (backend == nullptr) {
    pbl_shell_error(sh, "unknown backend '%s'", argv[1]);
    return -ENOENT;
  }

  if (argc > 2) {
    long level;
    if (pbl_shell_strtol(argv[2], &level) != 0) {
      pbl_shell_error(sh, "invalid level '%s'", argv[2]);
      return -EINVAL;
    }
    pbl_log_backend_set_level(backend, level);
  }

  pbl_shell_print(sh, "%s: level %i", backend->name, backend->state->level);
  return 0;
}

PBL_SHELL_SUBCMD_SET_CREATE(sub_log);
PBL_SHELL_CMD_REGISTER(log, sub_log, "Logging", nullptr);
PBL_SHELL_SUBCMD_ADD(sub_log, backends, nullptr, "List the backends", prv_cmd_backends, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_log, level, nullptr, "Get or set a backend level <backend> [level]",
                     prv_cmd_level, 2, 1);
#endif
