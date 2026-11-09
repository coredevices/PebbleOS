/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/debug.h>
#include <pbl/logging/backend.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/endpoint.h>
#include <pbl/pp/send.h>
#include <pbl/pp/session.h>
#include <pbl/util/byteorder.h>

#include <applib/app_logging.h>
#include <logging/logging_private.h>

PBL_LOG_MODULE_DEFINE(service_app_log_endpoint, CONFIG_SERVICE_APP_LOG_ENDPOINT_LOG_LEVEL);

static const uint32_t MIN_STACK_FOR_SEND_DATA = 400;

static void prv_backend_process(const struct pbl_log_backend *backend,
                                const struct pbl_log_msg *msg) {
  if (!msg->app_uuid || pbl_stack_free() <= MIN_STACK_FOR_SEND_DATA) {
    return;
  }

  struct pbl_pp_session *session = pbl_pp_session_get_system();
  if (!session) {
    return;
  }

  LogBinaryMessage header = {
    .timestamp = pbl_cpu_to_be32(msg->timestamp),
    .log_level = msg->level,
    .message_length = msg->text_len,
    .line_number = pbl_cpu_to_be16(msg->line),
  };
  strncpy(header.filename, msg->file, sizeof(header.filename));

  const size_t length = sizeof(Uuid) + sizeof(header) + msg->text_len;
  struct pbl_pp_send_buffer *sb =
      pbl_pp_send_buffer_begin(session, PBL_PP_ENDPOINT_APP_LOG, length, PBL_PP_DEFAULT_TIMEOUT_MS);
  if (!sb) {
    return;
  }

  pbl_pp_send_buffer_write(sb, msg->app_uuid, sizeof(Uuid));
  pbl_pp_send_buffer_write(sb, (const uint8_t *)&header, sizeof(header));
  pbl_pp_send_buffer_write(sb, (const uint8_t *)msg->text, msg->text_len);
  pbl_pp_send_buffer_end(sb);
}

static const struct pbl_log_backend_api s_api = {
  .process = prv_backend_process,
};

// Enabled by the phone.
PBL_LOG_BACKEND_DEFINE(app_log, s_api, LOG_LEVEL_DEBUG_VERBOSE, false);

bool app_log_is_bt_enabled(void) {
  return pbl_log_backend_is_enabled(PBL_LOG_BACKEND_GET(app_log));
}

void app_log_protocol_msg_callback(struct pbl_pp_session *session, const uint8_t *data,
                                   const size_t length) {
  typedef struct PBL_PACKED AppLogCommand {
    uint8_t commandType;
  } AppLogCommand;

  enum AppLogCommandType {
    APP_LOG_COMMAND_DISABLE_LOGGING = 0,
    APP_LOG_COMMAND_ENABLE_LOGGING = 1,
  };

  AppLogCommand *command = (AppLogCommand *)data;
  switch (command->commandType) {
    case APP_LOG_COMMAND_ENABLE_LOGGING:
      pbl_log_backend_enable(PBL_LOG_BACKEND_GET(app_log));
      break;
    case APP_LOG_COMMAND_DISABLE_LOGGING:
      pbl_log_backend_disable(PBL_LOG_BACKEND_GET(app_log));
      break;
    default:
      PBL_LOG_WRN("Invalid app log command 0x%x", command->commandType);
  }
}

#ifndef CONFIG_RECOVERY_FW
PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_APP_LOG, PBL_PP_ACCESS_PRIVATE,
                       app_log_protocol_msg_callback);
#endif
