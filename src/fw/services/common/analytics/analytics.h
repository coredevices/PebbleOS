/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <inttypes.h>

#include "kernel/pebble_tasks.h"
#include "util/uuid.h"
#include "analytics_metric_table.h"
#include "analytics_event.h"


#define ANALYTICS_LOG_DEBUG(fmt, args...) \
            PBL_LOG_D(LOG_DOMAIN_ANALYTICS, LOG_LEVEL_DEBUG, fmt, ## args)

//! Possible values for the client argument when setting/updating a metric. This tells the
//! analytics code under which "blob" to put the metric. For device metrics, the client argument
//! is ignored, but passing in AnalyticsClient_System is basically good documentation.
//! For app metrics, the client can be AnalyticsClient_App, AnalyticsClient_Worker or
//! AnalyticsClient_CurrentTask
typedef enum AnalyticsClient {
  AnalyticsClient_System,      //! Put in the "device" blob. Illegal if the metric is an app metric.
  AnalyticsClient_App,         //! Put in the "app" blob with the UUID of the current foreground app
  AnalyticsClient_Worker,      //! Put in the "app" blob with the UUID of the current background
                               //!   worker
  AnalyticsClient_CurrentTask, //! Put in the "app" blob with the UUID of the current task (either
                               //!   app or worker)
  AnalyticsClient_Ignore,      //! For internal use by the analytics module only
} AnalyticsClient;


void analytics_init(void);

//! Set a scalar metric
//! @param metric The metric to set
//! @param val The new value
//! @param client If the metric is an app metric, this logs it to the app blob using the UUID of the given client.
//!               If the metric is a device metric, client must be AnalyticsClient_System
void analytics_set(AnalyticsMetric metric, int64_t val, AnalyticsClient client);

//! Keeps val if it's larger than the previous measurement
//! @param metric The metric to set
//! @param val The value of the new measurement
//! @param client If the metric is an app metric, this logs it to the app blob using the UUID of the given client.
//!               If the metric is a device metric, client must be AnalyticsClient_System
void analytics_max(AnalyticsMetric metric, int64_t val, AnalyticsClient client);

//! Set a scalar metric for an app blob by UUID
//! @param metric The metric to set. This should be an app metric
//! @param val The new value
//! @param uuid The uuid of the app blob
void analytics_set_for_uuid(AnalyticsMetric metric, int64_t val, const Uuid *uuid);

//! Set an array metric
//! @param metric The metric to set
//! @param data The new data array
//! @param client If the metric is an app metric, this logs it to the app blob using the UUID of the given client.
//!               If the metric is a device metric, client must be AnalyticsClient_System
// TODO: Remove this, and add analytics_append_array or something. See PBL-5333
void analytics_set_entire_array(AnalyticsMetric metric, const void *data, AnalyticsClient client);

//! Increment a metric by 1
//! @param metric The metric to increment
//! @param client If the metric is an app metric, this logs it to the app blob using the UUID of the given client.
//!               If the metric is a device metric, client must be AnalyticsClient_System
void analytics_inc(AnalyticsMetric metric, AnalyticsClient client);

//! Increment an app metric for an app with the given UUID by 1
//! @param metric The metric to increment. This should be an app metric
//! @param uuid The uuid of the app blob
void analytics_inc_for_uuid(AnalyticsMetric metric, const Uuid *uuid);

//! Increment a metric
//! @param metric The metric to increment
//! @param amount The amount to increment by
//! @param client If the metric is an app metric, this logs it to the app blob using the UUID of the given client.
//!               If the metric is a device metric, client must be AnalyticsClient_System
void analytics_add(AnalyticsMetric metric, int64_t amount, AnalyticsClient client);

//! Increment an app metric for an app with the given UUID
//! @param metric The metric to increment. This should be an app metric
//! @param amount The amount to increment by
//! @param uuid The uuid of the app blob
void analytics_add_for_uuid(AnalyticsMetric metric, int64_t amount, const Uuid *uuid);

//! Starts a stopwatch that integrates a "rate of things" over time.
//! @param metric The metric of the stopwatch to start
//! @param client If the metric is an app metric, this logs it to the app blob using the UUID of the given client.
//!               If the metric is a device metric, client must be AnalyticsClient_System
void analytics_stopwatch_start(AnalyticsMetric metric, AnalyticsClient client);

//! Starts a stopwatch that integrates a "rate of things" over time.
//! @param metric The metric for which to start the stopwatch.
//! @param count_per_second The rate in number of things per second to count.
//! @param client If the metric is an app metric, this logs it to the app blob using the UUID of the given client.
//!               If the metric is a device metric, client must be AnalyticsClient_System
//! For example, if you want to measure "bytes transferred" over time and know the transfer speed is 1024 bytes per
//! second, then you would pass in 1024 as count_per_second.
void analytics_stopwatch_start_at_rate(AnalyticsMetric metric, uint32_t count_per_second, AnalyticsClient client);

//! Stops a stopwatch
//! @param metric The metric of the stopwatch
void analytics_stopwatch_stop(AnalyticsMetric metric);
