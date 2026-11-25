/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <nimble/transport/hci_h4.h>

/**
 * @brief Allocators for what the controller sends, for the H4 state machine.
 *
 * The state machine takes a missing buffer as a framing error, so these wait for the host to free
 * one. The transport backpressures the controller meanwhile.
 */
extern const struct hci_h4_allocators nimble_hci__allocs_from_ll;

/** @brief Set the allocators up. Call once, before the controller sends anything. */
void nimble_hci__alloc_init(void);
