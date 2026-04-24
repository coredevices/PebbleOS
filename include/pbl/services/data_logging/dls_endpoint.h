/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "dls_private.h"

#include <stdint.h>

void dls_endpoint_init(void);

void dls_endpoint_close_session(uint8_t session_id);

bool dls_endpoint_send_data(DataLoggingSession *logging_session, const uint8_t *data, unsigned int num_bytes);

bool dls_endpoint_open_session(DataLoggingSession *logging_session);

