/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "meta_endpoint.h"

#include "kernel/pbl_malloc.h"
#include "services/common/system_task.h"
#include "system/logging.h"
#include "util/net.h"

static const uint16_t META_ENDPOINT_ID = 0;

static void prv_send_meta_response_kernelbg_cb(void *data) {
  MetaResponseInfo *meta_response_info_heap_copy = data;

  // Swap endpoint_id bytes to be Big-Endian:
  meta_response_info_heap_copy->payload.endpoint_id =
          htons(meta_response_info_heap_copy->payload.endpoint_id);

  uint16_t payload_size;
  if (meta_response_info_heap_copy->payload.error_code == MetaResponseCodeCorruptedMessage) {
    payload_size = sizeof(meta_response_info_heap_copy->payload.error_code);
  } else {
    payload_size = sizeof(meta_response_info_heap_copy->payload);
  }

  comm_session_send_data(meta_response_info_heap_copy->session, META_ENDPOINT_ID,
                         (const uint8_t *)&meta_response_info_heap_copy->payload,
                         payload_size, COMM_SESSION_DEFAULT_TIMEOUT);

  kernel_free(meta_response_info_heap_copy);
}

void meta_endpoint_send_response_async(const MetaResponseInfo *meta_response_info) {
  PBL_LOG(LOG_LEVEL_ERROR, "Meta protocol error: 0x%x (endpoint=%u)",
          meta_response_info->payload.error_code, meta_response_info->payload.endpoint_id);

  MetaResponseInfo *meta_response_info_heap_copy = kernel_zalloc_check(sizeof(*meta_response_info));
  memcpy(meta_response_info_heap_copy, meta_response_info, sizeof(*meta_response_info));
  system_task_add_callback(prv_send_meta_response_kernelbg_cb, meta_response_info_heap_copy);
}

void meta_protocol_msg_callback(CommSession *session, const uint8_t* data, size_t length) {
  PBL_LOG(LOG_LEVEL_INFO, "Meta endpoint callback called");
}
