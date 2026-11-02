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
 * @brief Heart rate quality as the phone expects it, from an algorithm confidence.
 *
 * The 0-7 HeartRateQuality scale of the measurement logs, also used for the quality stored in
 * minute records.
 *
 * @param confidence Algorithm confidence, 0-100.
 * @param off_wrist The watch is off the wrist.
 * @return HeartRateQuality value.
 */
uint8_t protobuf_log_hr_quality(uint8_t confidence, bool off_wrist);

/**
 * @brief Log a heart rate sample.
 *
 * @param ref Session created with protobuf_log_hr_create().
 * @param sample_utc Sample UTC time.
 * @param bpm Heart rate in beats per minute.
 * @param confidence Algorithm confidence, 0-100.
 * @param off_wrist The watch is off the wrist.
 * @return true on success.
 */
bool protobuf_log_hr_add_sample(ProtobufLogRef ref, time_t sample_utc, uint8_t bpm,
                                uint8_t confidence, bool off_wrist);

/** @} */
