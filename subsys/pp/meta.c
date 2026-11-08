/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/util/byteorder.h>

PBL_LOG_MODULE_DECLARE(pp, CONFIG_PP_LOG_LEVEL);

struct pbl_pp__meta_response {
  struct pbl_pp_session *session;
  struct PBL_PACKED {
    uint8_t error;
    uint16_t endpoint_id;
  } payload;
};

static void prv_send_cb(void *data) {
  struct pbl_pp__meta_response *response = data;
  const size_t length = (response->payload.error == PBL_PP__META_CORRUPTED)
                            ? sizeof(response->payload.error)
                            : sizeof(response->payload);

  (void)pbl_pp_send(response->session, PBL_PP_ENDPOINT_META, (const uint8_t *)&response->payload,
                    length, PBL_PP_DEFAULT_TIMEOUT_MS);
  pbl_free(response);
}

void pbl_pp__meta_send_error(struct pbl_pp_session *session, enum pbl_pp__meta_error error,
                             uint16_t endpoint_id) {
  struct pbl_pp__meta_response *response;

  PBL_LOG_ERR("Meta protocol error: 0x%x (endpoint=%u)", error, endpoint_id);

  response = pbl_zalloc_check(sizeof(*response));
  *response = (struct pbl_pp__meta_response){
    .session = session,
    .payload = {.error = error, .endpoint_id = pbl_cpu_to_be16(endpoint_id)},
  };
  pbl_sys_work_call(prv_send_cb, response);
}

static void prv_meta_handler(struct pbl_pp_session *session, const uint8_t *data, size_t length) {
  PBL_LOG_DBG("Meta message received");
}

PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_META, PBL_PP_ACCESS_ANY, prv_meta_handler);
