# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_include_directories(.)
pbl_add_subdirectory_ifdef(CONFIG_BT bluetooth)
pbl_add_subdirectory_ifdef(CONFIG_CRON cron)
add_subdirectory(logging)
add_subdirectory(task_wdt)
