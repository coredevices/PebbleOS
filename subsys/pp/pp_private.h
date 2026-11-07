/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/pp/endpoint.h>
#include <pbl/pp/protocol.h>
#include <pbl/pp/send.h>
#include <pbl/pp/transport.h>
#include <pbl/util/list.h>
#include <pbl/util/uuid.h>

struct pbl_sem;

struct pbl_pp__receive_state {
  uint16_t bytes_received;
  uint16_t bytes_to_ignore;
  uint16_t payload_length;
  uint8_t header[sizeof(struct pbl_pp_header)];
  const struct pbl_pp_receiver_api *receiver_api;
  struct pbl_pp_receiver *receiver;
};

struct pbl_pp_session {
  ListNode node;
  const struct pbl_pp_transport *transport;
  struct pbl_pp_send_job *send_queue;
  struct pbl_pp__receive_state rx;
  Uuid app_uuid;
  uint32_t capabilities;
  enum pbl_pp_destination destination;
  bool send_next_pending;
};

enum pbl_pp__meta_error : uint8_t {
  PBL_PP__META_CORRUPTED = 0xd0,
  PBL_PP__META_UNHANDLED = 0xdc,
  PBL_PP__META_DISALLOWED = 0xdd,
};

// Protects the session list and each session's send queue, flags and capabilities.
void pbl_pp__lock(void);
void pbl_pp__unlock(void);
void pbl_pp__assert_locked(void);

// Locked.
bool pbl_pp__session_is_open(const struct pbl_pp_session *session);

bool pbl_pp__in_send_task(void);
void pbl_pp__send_next_now(struct pbl_pp_session *session);

// Locked.
void pbl_pp__send_queue_drop(struct pbl_pp_session *session);

void pbl_pp__receive_drop(struct pbl_pp_session *session);

void pbl_pp__meta_send_error(struct pbl_pp_session *session, enum pbl_pp__meta_error error,
                             uint16_t endpoint_id);

void pbl_pp__version_request(struct pbl_pp_session *session);

#if UNITTEST
bool pbl_pp__send_next_is_pending(const struct pbl_pp_session *session);
struct pbl_sem *pbl_pp__send_buffer_semaphore(void);
struct pbl_sem *pbl_pp__send_buffer_bulk_semaphore(void);
void pbl_pp__send_buffer_reset(void);
#endif
