/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Sniff mode for the phone's link; on the Zephyr thread.

#include <stdint.h>

#include <zephyr/bluetooth/conn.h>

//! Allow sniff on the links to come; after bt_enable().
int zbt_power_init(void);

//! The phone's link is up (encrypted): allow sniff on it, and ask for it once idle.
void zbt_power_link_up(void);
void zbt_power_link_down(void);

//! The link is needed at full speed: leave sniff; back to it once idle again.
void zbt_power_busy(void);

//! The controller's Mode Change event.
void zbt_power_mode_changed(struct bt_conn *conn, uint8_t mode, uint16_t interval);
