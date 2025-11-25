/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "protobuf_log_hr.h"
#include "protobuf_log.h"

#include "services/common/hrm/hrm_manager.h"

#include "nanopb/measurements.pb.h"
#include "system/passert.h"

#include <util/size.h>

#include <stdint.h>
#include <stdbool.h>

// -----------------------------------------------------------------------------------------
// Convert HRMQuality to the internal protobuf representation.
T_STATIC uint32_t prv_hr_quality_int(HRMQuality quality) {
  switch (quality) {
    case HRMQuality_NoAccel:
      return pebble_pipeline_MeasurementSet_HeartRateQuality_NoAccel;
    case HRMQuality_OffWrist:
      return pebble_pipeline_MeasurementSet_HeartRateQuality_OffWrist;
    case HRMQuality_NoSignal:
      return pebble_pipeline_MeasurementSet_HeartRateQuality_NoSignal;
    case HRMQuality_Worst:
      return pebble_pipeline_MeasurementSet_HeartRateQuality_Worst;
    case HRMQuality_Poor:
      return pebble_pipeline_MeasurementSet_HeartRateQuality_Poor;
    case HRMQuality_Acceptable:
      return pebble_pipeline_MeasurementSet_HeartRateQuality_Acceptable;
    case HRMQuality_Good:
      return pebble_pipeline_MeasurementSet_HeartRateQuality_Good;
    case HRMQuality_Excellent:
      return pebble_pipeline_MeasurementSet_HeartRateQuality_Excellent;
  }
  WTF;    // Should never get here
  return 0;
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
                                HRMQuality quality) {
  uint32_t values[] = {bpm, prv_hr_quality_int(quality)};
  return protobuf_log_session_add_measurements(ref, sample_utc, ARRAY_LENGTH(values), values);
}
