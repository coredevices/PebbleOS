/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "protobuf_log.h"

#include <stdbool.h>
#include <stdint.h>

#include <pbl/services/hrm/hrm_manager.h>

#include <sys/types.h>

/**
 * @defgroup services_protobuf_log_protobuf_log_hr Heart rate log
 * @ingroup services_protobuf_log
 * @brief Heart rate samples logged as protobuf measurements.
 * @{
 */

/**
 * @brief Create a measurement session of BPM and heart rate quality.
 *
 * @param transport Transport, or NULL to log to data logging.
 * @return Session, or NULL on error.
 */
ProtobufLogRef protobuf_log_hr_create(ProtobufLogTransportCB transport);

/**
 * @brief Log a heart rate sample.
 *
 * @param ref Session created with protobuf_log_hr_create().
 * @param sample_utc Sample UTC time.
 * @param bpm Heart rate in beats per minute.
 * @param quality Signal quality.
 * @return true on success.
 */
bool protobuf_log_hr_add_sample(ProtobufLogRef ref, time_t sample_utc, uint8_t bpm,
                                HRMQuality quality);

/** @} */
