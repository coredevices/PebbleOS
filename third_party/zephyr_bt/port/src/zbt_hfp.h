/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Hands-free on the Zephyr host; all on the Zephyr thread.

#include <zephyr/bluetooth/conn.h>
#include <zephyr/net_buf.h>

//! Register the hands-free unit (SDP record, SCO); after bt_enable().
int zbt_hfp_init(void);

//! The BR/EDR link to the phone got encrypted: set the hands-free connection up.
void zbt_hfp_link_encrypted(struct bt_conn *conn);

//! The BR/EDR link went down.
void zbt_hfp_link_down(struct bt_conn *conn);

//! The phone's BR/EDR link once encrypted, or NULL.
struct bt_conn *zbt_hfp_acl(void);

//! The SDP client's buffer pool, for the phone book client too (one lookup at a time).
struct net_buf_pool *zbt_hfp_sdp_pool(void);
