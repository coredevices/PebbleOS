/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/pp/send.h>
#include <pbl/pp/session.h>

PBL_WEAK bool pbl_pp_session_has_capability(const struct pbl_pp_session *session,
                                            enum pbl_pp_capability capability) {
  return true;
}

PBL_WEAK struct pbl_pp_session *pbl_pp_session_get_system(void) {
  // Don't return NULL, lots of code paths expect a valid session
  return (struct pbl_pp_session *)1;
}

PBL_WEAK bool pbl_pp_session_is_system(const struct pbl_pp_session *session) {
  return false;
}

PBL_WEAK int pbl_pp_send(struct pbl_pp_session *session, uint16_t endpoint_id, const uint8_t *data,
                         size_t length, uint32_t timeout_ms) {
  return 0;
}

PBL_WEAK uint32_t pbl_pp_session_get_capabilities(const struct pbl_pp_session *session) {
  return 0U;
}
