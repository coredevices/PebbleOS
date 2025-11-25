/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/comm_session/session.h"

void pp_ble_control_protocol_msg_callback(CommSession* session,
                                          const uint8_t *data,
                                          unsigned int length);
