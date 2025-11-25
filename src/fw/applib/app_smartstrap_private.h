/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_smartstrap.h"
#include "applib/event_service_client.h"
#include "util/attributes.h"
#include "util/list.h"
#include "util/mbuf.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct PACKED {
  //! Info used to subscribed to smartstrap events
  EventServiceInfo event_info;
  //! How many attributes the app has created
  uint32_t num_attributes;
  //! Handlers supplied by the app
  SmartstrapHandlers handlers;
  //! Timeout configurable by the app
  uint16_t timeout_ms;
  //! Whether or not this struct is initialized
  bool is_initialized;
} SmartstrapConnectionState;

void app_smartstrap_cleanup(void);
