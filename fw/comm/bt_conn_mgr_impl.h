/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Opaque Bluetooth connection manager struct. It is expected that Bluetooth
//! Handles will have a ptr to this in their handler
typedef struct ConnectionMgrInfo ConnectionMgrInfo;

//! Constructor/Destructor which bluetooth handles need to call to init &
//! cleanup the ConnectionMgrInfo struct
ConnectionMgrInfo *bt_conn_mgr_info_init(void);
void bt_conn_mgr_info_deinit(ConnectionMgrInfo **info);
