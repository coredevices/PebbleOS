/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/services/hrm/hrm_manager.h>
#include <pbl/services/protobuf_log/protobuf_log.h>
#include <pbl/services/protobuf_log/protobuf_log_hr.h>
#include <pbl/util/size.h>
#include <pbl/util/testing.h>

#include <measurements.pb.h>

// -----------------------------------------------------------------------------------------
uint8_t protobuf_log_hr_quality(uint8_t confidence, bool off_wrist) {
  if (off_wrist) {
    return pebble_pipeline_MeasurementSet_HeartRateQuality_OffWrist;
  } else if (confidence >= 90) {
    return pebble_pipeline_MeasurementSet_HeartRateQuality_Excellent;
  } else if (confidence >= HRM_CONFIDENCE_HIGH) {
    return pebble_pipeline_MeasurementSet_HeartRateQuality_Good;
  } else if (confidence >= 60) {
    return pebble_pipeline_MeasurementSet_HeartRateQuality_Acceptable;
  } else if (confidence >= 40) {
    return pebble_pipeline_MeasurementSet_HeartRateQuality_Poor;
  }
  return pebble_pipeline_MeasurementSet_HeartRateQuality_Worst;
}

ProtobufLogRef protobuf_log_hr_create(ProtobufLogTransportCB transport) {
  // Create a measure log session, which we use to send heart rate readings to the phone
  ProtobufLogMeasurementType measure_types[] = {
    ProtobufLogMeasurementType_BPM,
    ProtobufLogMeasurementType_HRQuality,
  };

  ProtobufLogConfig log_config = {
    .type = ProtobufLogType_Measurements,
    .measurements = {
      .types = measure_types,
      .num_types = ARRAY_LENGTH(measure_types),
    },
  };

  return protobuf_log_create(&log_config, transport, 0 /*max_encoded_msg_size*/);
}

bool protobuf_log_hr_add_sample(ProtobufLogRef ref, time_t sample_utc, uint8_t bpm,
                                uint8_t confidence, bool off_wrist) {
  uint32_t values[] = {bpm, protobuf_log_hr_quality(confidence, off_wrist)};
  return protobuf_log_session_add_measurements(ref, sample_utc, ARRAY_LENGTH(values), values);
}
