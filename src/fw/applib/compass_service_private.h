/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "compass_service.h"

#include "applib/app_timer.h"
#include "applib/event_service_client.h"


typedef struct __attribute__((__packed__)) {
  CompassHeading compass_filter;
  int32_t last_angle;
  CompassHeading heading_declination;

  AppTimer* peek_timer;
  CompassHeadingHandler compass_cb;

  EventServiceInfo info;
} CompassServiceConfig;
