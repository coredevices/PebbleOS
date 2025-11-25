/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "protobuf_log.h"

#include "services/common/hrm/hrm_manager.h"

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

ProtobufLogRef protobuf_log_hr_create(ProtobufLogTransportCB transport);

bool protobuf_log_hr_add_sample(ProtobufLogRef ref, time_t sample_utc, uint8_t bpm,
                                HRMQuality quality);
