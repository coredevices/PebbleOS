/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/comm_session/session_send_buffer.h"

#include <stdbool.h>

void fake_session_send_buffer_init(void);

void fake_session_send_buffer_set_simulate_oom(bool enabled);

SendBuffer *fake_session_send_buffer_get_buffer(void);
