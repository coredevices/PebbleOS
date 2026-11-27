/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <pbl/kernel/compiler.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/main.h>
#include <pbl/util/byteorder.h>

PBL_LOG_MODULE_DECLARE(pp, CONFIG_PP_LOG_LEVEL);

enum : uint8_t {
  VERSION_REQUEST = 0x00,
  VERSION_RESPONSE = 0x01,
};

struct PBL_PACKED version_response_v1 {
  uint32_t pebble_library_version;
  uint32_t session_capabilities_bitfield;
  uint32_t platform_bitfield;
};

struct PBL_PACKED version_response_v2 {
  struct version_response_v1 v1;
  uint8_t response_version;
  uint8_t major_version;
  uint8_t minor_version;
  uint8_t bugfix_version;
};

struct PBL_PACKED version_response_v3 {
  struct version_response_v2 v2;
  uint32_t protocol_capabilities;
};

static void prv_request_cb(void *data) {
  static const uint8_t request = VERSION_REQUEST;

  (void)pbl_pp_send(data, PBL_PP_ENDPOINT_REMOTE_VERSION, &request, sizeof(request),
                    PBL_PP_DEFAULT_TIMEOUT_MS);
}

void pbl_pp__version_request(struct pbl_pp_session *session) {
  pbl_pp_main_call(prv_request_cb, session);
}

static void prv_handle_response(struct pbl_pp_session *session, const uint8_t *data,
                                size_t length) {
  const struct version_response_v3 *response = (const struct version_response_v3 *)data;
  uint32_t capabilities = 0U;

  if (length < sizeof(struct version_response_v1)) {
    PBL_LOG_ERR("Invalid version response");
    return;
  }

  // The 1.x Android app repeats its response, so a v1 response may be as long as a v2 one.
  if (length >= sizeof(struct version_response_v2)) {
    PBL_LOG_DBG("Connected to Mobile App %" PRIu8 ".%" PRIu8 "-%" PRIu8, response->v2.major_version,
                response->v2.minor_version, response->v2.bugfix_version);
    if (length >= sizeof(struct version_response_v3)) {
      capabilities = response->protocol_capabilities;
    }
  }

  pbl_pp_session_set_capabilities(session, capabilities);

  const uint32_t platform_bits = pbl_be32_to_cpu(response->v2.v1.platform_bitfield);

  PBL_LOG_INFO("Phone app: plf=0x%" PRIx32 ", capabilities=0x%" PRIx32, platform_bits,
               capabilities);

  pbl_pp__session_set_remote_os(session,
                                (enum pbl_pp_remote_os)(platform_bits & PBL_PP_REMOTE_OS_MASK));
}

static void prv_version_handler(struct pbl_pp_session *session, const uint8_t *data,
                                size_t length) {
  if (length < 1U || data[0] != VERSION_RESPONSE) {
    PBL_LOG_ERR("Invalid version message");
    return;
  }

  prv_handle_response(session, &data[1], length - 1U);
}

PBL_PP_ENDPOINT_DEFINE_MAIN(PBL_PP_ENDPOINT_REMOTE_VERSION, prv_version_handler);
