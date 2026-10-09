/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/services/data_logging/dls_private.h>
#include <pbl/services/hrm/hrm_manager.h>

#include <system/version.h>

// Generated from fw/services/protobuf_log/proto
#include <event.pb.h>
#include <measurements.pb.h>

/**
 * @defgroup services_protobuf_log Protobuf log
 * @ingroup services
 * @brief Protobuf payloads of measurements or events sent to the phone.
 *
 * Samples are accumulated into a @c pebble_pipeline_Payload (schemas in
 * @c fw/services/protobuf_log/proto) until it fills a record, which is then handed to the
 * session's transport. The default transport logs each record, prefixed by a @ref PLogMessageHdr
 * and zero padded to @ref PLOG_DLS_RECORD_SIZE, to a buffered @ref services_data_logging session
 * with tag @c DlsSystemTagProtobufLogSession.
 *
 * Measurement sessions log samples with a fixed set of values, such as heart rate BPM and
 * quality. Event sessions log @c pebble_pipeline_Event, such as activity sessions.
 *
 * @code{.c}
 * ProtobufLogMeasurementType types[] = {
 *   ProtobufLogMeasurementType_BPM,
 *   ProtobufLogMeasurementType_HRQuality,
 * };
 * ProtobufLogConfig config = {
 *   .type = ProtobufLogType_Measurements,
 *   .measurements = { .num_types = ARRAY_LENGTH(types), .types = types },
 * };
 * ProtobufLogRef log = protobuf_log_create(&config, NULL, 0);
 *
 * uint32_t values[] = { bpm, quality };
 * protobuf_log_session_add_measurements(log, rtc_get_time(), ARRAY_LENGTH(values), values);
 *
 * protobuf_log_session_delete(log);
 * @endcode
 * @{
 */

/** @brief Measurement type. */
typedef pebble_pipeline_MeasurementSet_Type ProtobufLogMeasurementType;
/** @brief Activity type. */
typedef pebble_pipeline_ActivityType_InternalType ProtobufLogActivityType;

/** @brief Time in milliseconds. */
#define ProtobufLogMeasurementType_TimeMS pebble_pipeline_MeasurementSet_Type_TimeMS
/** @brief Vector magnitude counts, a unitless measure of total activity. */
#define ProtobufLogMeasurementType_VMC pebble_pipeline_MeasurementSet_Type_VMC
/** @brief Accumulated steps. */
#define ProtobufLogMeasurementType_Steps pebble_pipeline_MeasurementSet_Type_Steps
/** @brief Distance in centimeters. */
#define ProtobufLogMeasurementType_DistanceCM pebble_pipeline_MeasurementSet_Type_DistanceCM
/** @brief Resting calories, in gram calories. */
#define ProtobufLogMeasurementType_RestingGCalories \
  pebble_pipeline_MeasurementSet_Type_RestingGCalories
/** @brief Active calories, in gram calories. */
#define ProtobufLogMeasurementType_ActiveGCalories \
  pebble_pipeline_MeasurementSet_Type_ActiveGCalories
/** @brief Heart rate in beats per minute. */
#define ProtobufLogMeasurementType_BPM pebble_pipeline_MeasurementSet_Type_BPM
/** @brief Heart rate variability, peak to peak time in milliseconds. */
#define ProtobufLogMeasurementType_RR pebble_pipeline_MeasurementSet_Type_RR
/** @brief Orientation. */
#define ProtobufLogMeasurementType_Orientation pebble_pipeline_MeasurementSet_Type_Orientation
/** @brief Ambient light. */
#define ProtobufLogMeasurementType_Light pebble_pipeline_MeasurementSet_Type_Light
/** @brief Watch temperature in kelvin. */
#define ProtobufLogMeasurementType_Temperature pebble_pipeline_MeasurementSet_Type_Temperature
/** @brief Heart rate signal quality. */
#define ProtobufLogMeasurementType_HRQuality pebble_pipeline_MeasurementSet_Type_HRQuality

/** @brief Unknown activity. */
#define ProtobufLogActivityType_UnknownType pebble_pipeline_ActivityType_InternalType_UnknownType
/** @brief Sleep. */
#define ProtobufLogActivityType_Sleep pebble_pipeline_ActivityType_InternalType_Sleep
/** @brief Deep sleep. */
#define ProtobufLogActivityType_DeepSleep pebble_pipeline_ActivityType_InternalType_DeepSleep
/** @brief Nap. */
#define ProtobufLogActivityType_Nap pebble_pipeline_ActivityType_InternalType_Nap
/** @brief Deep nap. */
#define ProtobufLogActivityType_DeepNap pebble_pipeline_ActivityType_InternalType_DeepNap
/** @brief Walk. */
#define ProtobufLogActivityType_Walk pebble_pipeline_ActivityType_InternalType_Walk
/** @brief Run. */
#define ProtobufLogActivityType_Run pebble_pipeline_ActivityType_InternalType_Run
/** @brief Open workout. */
#define ProtobufLogActivityType_Open pebble_pipeline_ActivityType_InternalType_Open

/** @brief Size of a buffer holding the payload sender ID, the watch serial number. */
#define PLOG_MAX_SENDER_ID_LEN 64
/** @brief Size of a buffer holding the payload sender type. */
#define PLOG_MAX_SENDER_TYPE_LEN 64
/** @brief Size of a buffer holding the payload sender version patch string. */
#define PLOG_MAX_SENDER_VERSION_PATCH_LEN FW_METADATA_VERSION_TAG_BYTES
/** @brief Payload sender type. */
#define PLOG_PAYLOAD_SENDER_TYPE "watch"

/** @brief Size of the data logging records, and maximum encoded message size. */
#define PLOG_DLS_RECORD_SIZE DLS_SESSION_MAX_BUFFERED_ITEM_SIZE

/** @brief Session payload type. */
typedef enum ProtobufLogType {
  /** Samples of a fixed set of measurements. */
  ProtobufLogType_Measurements,
  /** @c pebble_pipeline_Event items. */
  ProtobufLogType_Events,
} ProtobufLogType;

/** @brief Session configuration. */
typedef struct ProtobufLogConfig {
  /** Payload type. */
  ProtobufLogType type;
  union {
    /** Configuration of @ref ProtobufLogType_Measurements sessions. */
    struct {
      /** Number of values in each sample. */
      uint8_t num_types;
      /** Type of each value, @ref num_types entries. Copied by protobuf_log_create(). */
      ProtobufLogMeasurementType *types;
    } measurements;
    /** Configuration of @ref ProtobufLogType_Events sessions, empty. */
    struct {
      // empty for now
    } events;
  };
} ProtobufLogConfig;

/** @brief Session handle. */
typedef void *ProtobufLogRef;

/**
 * @brief Transport sending an encoded record.
 *
 * @param buffer Record, a @ref PLogMessageHdr followed by the encoded payload. The buffer is
 *               @ref PLOG_DLS_RECORD_SIZE bytes long and may be padded in place.
 * @param buf_size Record size in bytes.
 * @return true on success.
 */
typedef bool (*ProtobufLogTransportCB)(uint8_t *buffer, size_t buf_size);

/**
 * @brief Initialize the service.
 *
 * @return true.
 */
bool protobuf_log_init(void);

/**
 * @brief Create a session.
 *
 * @param config Session configuration.
 * @param transport Transport, or NULL to log to data logging.
 * @param max_encoded_msg_size Maximum record size, at most @ref PLOG_DLS_RECORD_SIZE. 0, the
 *                             usual value, selects @ref PLOG_DLS_RECORD_SIZE.
 * @return Session, or NULL on error.
 */
ProtobufLogRef protobuf_log_create(ProtobufLogConfig *config, ProtobufLogTransportCB transport,
                                   size_t max_encoded_msg_size);

/**
 * @brief Add a sample to a measurement session.
 *
 * The accumulated data is encoded and sent once a record is full.
 *
 * @param session Session.
 * @param sample_utc Sample UTC time.
 * @param num_values Number of entries in @p values, equal to the configured number of types.
 * @param values Sample values, in the configured type order.
 * @return true on success.
 */
bool protobuf_log_session_add_measurements(ProtobufLogRef session, time_t sample_utc,
                                           uint32_t num_values, uint32_t *values);

/**
 * @brief Add an event to an event session.
 *
 * The accumulated data is encoded and sent once a record is full.
 *
 * @param session_ref Session.
 * @param event Event.
 * @return true on success.
 */
bool protobuf_log_session_add_event(ProtobufLogRef session_ref, pebble_pipeline_Event *event);

/**
 * @brief Encode and send the data accumulated so far.
 *
 * The session is restarted even if sending fails.
 *
 * @param session Session.
 * @return true on success.
 */
bool protobuf_log_session_flush(ProtobufLogRef session);

/**
 * @brief Flush and free a session.
 *
 * @param session Session, may be NULL.
 * @return true.
 */
bool protobuf_log_session_delete(ProtobufLogRef session);

/** @} */
