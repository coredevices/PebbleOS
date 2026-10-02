/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "protobuf_log.h"
#include "pbl/services/activity/activity.h"

#include <stdint.h>

/**
 * @defgroup services_protobuf_log_protobuf_log_test Payload decoding
 * @ingroup services_protobuf_log
 * @brief Decoders of encoded payloads, for debugging and unit tests.
 * @{
 */

/**
 * @brief Decode a payload of measurement sets.
 *
 * @param[out] type Payload type.
 * @param encoded_buf Encoded payload.
 * @param encoded_buf_size Size of @p encoded_buf in bytes.
 * @param[out] payload_sender_type Sender type string.
 * @param[out] payload_sender_id Sender ID string.
 * @param[out] payload_sender_version_patch Sender version patch string.
 * @param[out] payload_send_time Payload send time.
 * @param[out] payload_sender_v_major Sender major version, may be NULL.
 * @param[out] payload_sender_v_minor Sender minor version, may be NULL.
 * @param[out] uuid Measurement set UUID.
 * @param[out] time_utc Measurement set start UTC time.
 * @param[out] time_end_utc Measurement set end UTC time.
 * @param[out] utc_to_local Offset from UTC to local time.
 * @param[in,out] num_types Capacity of @p types on entry, number of types on exit.
 * @param[out] types Measurement types.
 * @param[in,out] num_samples Capacity of @p offset_sec on entry, number of samples on exit.
 * @param[out] offset_sec Offset of each sample in seconds.
 * @param[in,out] num_values Capacity of @p values on entry, number of values on exit.
 * @param[out] values Values, @p num_types per sample.
 * @return true on success.
 */
bool protobuf_log_private_mset_decode(
    ProtobufLogType *type, void *encoded_buf, uint32_t encoded_buf_size,
    char payload_sender_type[PLOG_MAX_SENDER_TYPE_LEN],
    char payload_sender_id[PLOG_MAX_SENDER_ID_LEN],
    char payload_sender_version_patch[FW_METADATA_VERSION_TAG_BYTES], uint32_t *payload_send_time,
    uint32_t *payload_sender_v_major, uint32_t *payload_sender_v_minor, Uuid *uuid,
    uint32_t *time_utc, uint32_t *time_end_utc, int32_t *utc_to_local, uint32_t *num_types,
    ProtobufLogMeasurementType *types, uint32_t *num_samples, uint32_t *offset_sec,
    uint32_t *num_values, uint32_t *values);

/**
 * @brief Decode a payload of events.
 *
 * @param[out] type Payload type.
 * @param encoded_buf Encoded payload.
 * @param encoded_buf_size Size of @p encoded_buf in bytes.
 * @param[out] payload_sender_type Sender type string.
 * @param[out] payload_sender_id Sender ID string.
 * @param[out] payload_sender_version_patch Sender version patch string.
 * @param[out] payload_send_time Payload send time.
 * @param[out] payload_sender_v_major Sender major version, may be NULL.
 * @param[out] payload_sender_v_minor Sender minor version, may be NULL.
 * @param[in,out] num_events Capacity of @p events on entry, number of events on exit.
 * @param[out] events Events.
 * @param[out] event_uuids UUID of each event.
 * @param[in,out] num_sessions Capacity of @p sessions on entry, number of activity sessions on
 *                exit.
 * @param[out] sessions Activity sessions, indexed like @p events: the session of event @c i is
 *             at @c sessions[i].
 * @return true on success.
 */
bool protobuf_log_private_events_decode(
    ProtobufLogType *type, void *encoded_buf, uint32_t encoded_buf_size,
    char payload_sender_type[PLOG_MAX_SENDER_TYPE_LEN],
    char payload_sender_id[PLOG_MAX_SENDER_ID_LEN],
    char payload_sender_version_patch[FW_METADATA_VERSION_TAG_BYTES], uint32_t *payload_send_time,
    uint32_t *payload_sender_v_major, uint32_t *payload_sender_v_minor, uint32_t *num_events,
    pebble_pipeline_Event *events, Uuid *event_uuids, uint32_t *num_sessions,
    ActivitySession *sessions);

/** @} */
