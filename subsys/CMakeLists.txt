# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_include_directories(.)
pbl_add_subdirectory_ifdef(CONFIG_BT bluetooth)
add_subdirectory(crc)
pbl_add_subdirectory_ifdef(CONFIG_CRON cron)
pbl_add_subdirectory_ifdef(CONFIG_INPUT input)
add_subdirectory(logging)
pbl_add_subdirectory_ifdef(CONFIG_SHELL shell)
add_subdirectory(task_wdt)
