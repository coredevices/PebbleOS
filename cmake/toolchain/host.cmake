# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Host toolchain, for boards that run the firmware as a native application.

find_program(CMAKE_C_COMPILER NAMES clang cc gcc REQUIRED)
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
