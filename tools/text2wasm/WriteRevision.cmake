# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${PEBBLEOS_ROOT}" rev-parse HEAD
  OUTPUT_VARIABLE revision
  COMMAND_ERROR_IS_FATAL ANY)
file(CONFIGURE OUTPUT "${OUTPUT}" CONTENT "${revision}" @ONLY)
