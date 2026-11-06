/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @file partitions.h
//! The partitions of the board's flash. A partition called X has PART_X for the partition
//! functions, and PART_X_OFFSET and PART_X_SIZE for constant expressions. A board that does not
//! have a partition does not define them.

#include <pbl/drivers/flash/partition.h>

#ifdef CONFIG_BOARD_PARTITIONS_DUAL_SLOT
#include "partitions_dual_slot.h"
#elifdef CONFIG_BOARD_PARTITIONS_SINGLE_SLOT
#include "partitions_single_slot.h"
#endif
