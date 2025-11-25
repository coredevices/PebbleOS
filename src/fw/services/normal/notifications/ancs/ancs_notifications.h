/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdlib.h>

#include "comm/ble/kernel_le_client/ancs/ancs_types.h"
#include "services/normal/timeline/item.h"


void ancs_notifications_handle_message(uint32_t uid,
                                       ANCSProperty properties,
                                       ANCSAttribute **notif_attributes,
                                       ANCSAttribute **app_attributes);

void ancs_notifications_handle_notification_removed(uint32_t ancs_uid, ANCSProperty properties);
