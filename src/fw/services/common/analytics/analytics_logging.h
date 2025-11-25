/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "analytics_event.h"

typedef enum {
  ANALYTICS_BLOB_KIND_DEVICE_HEARTBEAT,
  ANALYTICS_BLOB_KIND_APP_HEARTBEAT,
  ANALYTICS_BLOB_KIND_EVENT
} AnalyticsBlobKind;

void analytics_logging_init(void);

//! Used internally to log raw analytics events
void analytics_logging_log_event(AnalyticsEventBlob *event_blob);

//! Exposed for unit testing only
void analytics_logging_system_task_cb(void *ignored);
