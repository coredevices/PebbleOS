/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "dls_private.h"

#include <stdint.h>

/**
 * @defgroup services_data_logging_dls_endpoint Data logging endpoint
 * @ingroup services_data_logging
 * @brief Pebble Protocol endpoint exchanging session data with the phone.
 *
 * Messages are sent over the system session; when there is none the functions do nothing.
 * @{
 */

/** @brief Initialize the endpoint state. */
void dls_endpoint_init(void);

/**
 * @brief Tell the phone a session is closed.
 *
 * @param session_id Session ID.
 */
void dls_endpoint_close_session(uint8_t session_id);

/**
 * @brief Send a chunk of session data to the phone.
 *
 * Moves the session to the sending state until the phone acks. Does nothing if the session is
 * still waiting for an ack.
 *
 * @param logging_session Session the data belongs to.
 * @param data Whole items read from the session storage.
 * @param num_bytes Length of @p data in bytes, at most @c DLS_ENDPOINT_MAX_PAYLOAD.
 * @return false if there is no system session or no send buffer is available, true otherwise.
 */
bool dls_endpoint_send_data(DataLoggingSession *logging_session, const uint8_t *data,
                            unsigned int num_bytes);

/**
 * @brief Announce a session to the phone.
 *
 * Moves the session to the opening state until the phone acks.
 *
 * @param logging_session Session to open.
 * @return true if the open message was sent.
 */
bool dls_endpoint_open_session(DataLoggingSession *logging_session);

/** @} */
