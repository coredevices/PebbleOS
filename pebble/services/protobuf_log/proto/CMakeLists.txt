# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_include_directories(${CMAKE_CURRENT_BINARY_DIR})

pbl_library_named(protobuf_log_schemas)
pbl_library_kind(stlib)
pbl_library_nanopb_sources(
  activity.proto
  common.proto
  event.proto
  measurements.proto
  payload.proto
)
