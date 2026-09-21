# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_include_directories(.)
add_subdirectory(bluetooth)
pbl_add_subdirectory_ifdef(CONFIG_CRON cron)
add_subdirectory(logging)
