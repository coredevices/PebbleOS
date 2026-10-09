/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "protobuf_log.h"

#include <pbl/services/activity/activity.h>

/**
 * @defgroup services_protobuf_log_protobuf_log_activity_sessions Activity session log
 * @ingroup services_protobuf_log
 * @brief Activity sessions logged as protobuf events.
 * @{
 */

/**
 * @brief Create an event session logging to data logging.
 *
 * @return Session, or NULL on error.
 */
ProtobufLogRef protobuf_log_activity_sessions_create(void);

/**
 * @brief Log an activity session.
 *
 * Currently a no-op.
 *
 * @param ref Session created with protobuf_log_activity_sessions_create().
 * @param sample_utc Event creation UTC time.
 * @param session Activity session.
 * @return true.
 */
bool protobuf_log_activity_sessions_add(ProtobufLogRef ref, time_t sample_utc,
                                        ActivitySession *session);

/**
 * @brief Convert a decoded activity session event.
 *
 * @param event_in Event holding an activity session.
 * @param[out] session_out Activity session.
 * @return true.
 */
bool protobuf_log_activity_sessions_decode(pebble_pipeline_Event *event_in,
                                           ActivitySession *session_out);

/** @} */
