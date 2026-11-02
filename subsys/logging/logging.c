/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "logging_private.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/irq.h>
#include <pbl/kernel/sched.h>
#include <pbl/kernel/syscall.h>
#include <pbl/logging/backend.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/interrupts.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/string.h>

#include <debug/advanced_logging.h>

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

void pbl_log_backend_enable(const struct pbl_log_backend *backend) {
  __atomic_store_n(&backend->state->enabled, true, __ATOMIC_RELAXED);
}

void pbl_log_backend_disable(const struct pbl_log_backend *backend) {
  __atomic_store_n(&backend->state->enabled, false, __ATOMIC_RELAXED);
}

bool pbl_log_backend_is_enabled(const struct pbl_log_backend *backend) {
  return __atomic_load_n(&backend->state->enabled, __ATOMIC_RELAXED);
}

void pbl_log_backend_set_level(const struct pbl_log_backend *backend, int level) {
  __atomic_store_n(&backend->state->level, (int16_t)level, __ATOMIC_RELAXED);
}

const struct pbl_log_backend *pbl_log_backend_get_by_name(const char *name) {
  PRV_FOREACH_BACKEND(backend) {
    if (strcmp(backend->name, name) == 0) {
      return backend;
    }
  }

  return nullptr;
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
    if (backend->api->panic && __atomic_load_n(&backend->state->enabled, __ATOMIC_RELAXED)) {
      backend->api->panic(backend);
    }
  }
}

//! @param filtered The level was already checked against a runtime module level.
static void prv_dispatch(const struct pbl_log_msg *msg, bool filtered) {
  PRV_FOREACH_BACKEND(backend) {
    if (!__atomic_load_n(&backend->state->enabled, __ATOMIC_RELAXED)) {
      continue;
    }

    if (!filtered && msg->level != LOG_LEVEL_ALWAYS &&
        msg->level > __atomic_load_n(&backend->state->level, __ATOMIC_RELAXED)) {
      continue;
    }

    backend->api->process(backend, msg);
  }
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

static void prv_log_flash(LogBinaryMessage *log_message, uint32_t flags) {
  int length = sizeof(*log_message) + log_message->message_length;

  if (g_pbl_log_enabled &&
      ((flags & PBL_LOG_FLAG_FILTERED) || log_message->log_level == LOG_LEVEL_ALWAYS ||
       (log_message->log_level <= FLASH_LOG_LEVEL))) {
    pbl_log_advanced((const char *)log_message, length, flags & PBL_LOG_FLAG_ASYNC);
  }
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

  if (!pbl_irq_is_locked() && !mcu_state_is_isr() && !pbl_sched_is_locked()) {
    prv_log_flash(log_message, flags);
  }
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
