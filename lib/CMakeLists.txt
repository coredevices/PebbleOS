# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(c)
add_subdirectory(util)
pbl_add_subdirectory_ifdef(CONFIG_BTUTIL btutil)
pbl_add_subdirectory_ifdef(CONFIG_FUEL_GAUGE fuel_gauge)
pbl_add_subdirectory_ifdef(CONFIG_KRAEPELIN kraepelin)
