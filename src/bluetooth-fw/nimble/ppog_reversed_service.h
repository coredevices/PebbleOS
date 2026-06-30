/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <host/ble_gap.h>

void ppog_reversed_service_init(void);

//! Called from the advert.c GAP event dispatcher on BLE_GAP_EVENT_SUBSCRIBE.
//! Forwards CCCD changes on our notify characteristic to the kernel-side PPoG
//! state machine.
void ppog_reversed_service_handle_subscribe_event(struct ble_gap_event *event);

//! Called from the advert.c GAP event dispatcher on BLE_GAP_EVENT_NOTIFY_TX
//! when a previously-queued notification finished sending — used to drain any
//! tx backlog accumulated under BLE_HS_ENOMEM.
void ppog_reversed_service_handle_notify_tx_event(struct ble_gap_event *event);
