# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources upng/upng.c)
set(includes upng)
pbl_include_directories(${includes})
pbl_library_named(upng)
pbl_library_kind(stlib)
pbl_library_sources(${sources})
pbl_library_include_directories(.)
pbl_library_compile_definitions(UPNG_CONFIG_FILE="upng_config.h")
