# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_include_directories(.)

pbl_library()
pbl_library_sources(${PBL_BOARD_NAME}/${PBL_BOARD_NAME}.c)
