/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// No devicetree: the HCI driver is a fixed device provided by the port.

#pragma once

#include <zephyr/device.h>

extern const struct device zbt_hci_dev;

#define DT_HAS_CHOSEN(name)         DT_HAS_CHOSEN_##name
#define DT_HAS_CHOSEN_zephyr_bt_hci 1
#define DT_CHOSEN(name)             DT_CHOSEN_##name
#define DT_CHOSEN_zephyr_bt_hci     zbt_hci_node
#define DEVICE_DT_GET(node)         (&zbt_hci_dev)

#define DT_NODE_HAS_PROP(node_id, prop)                  0
#define DT_PROP_OR(node_id, prop, default_value)         (default_value)
#define DT_ENUM_IDX_OR(node_id, prop, default_idx_value) (default_idx_value)
