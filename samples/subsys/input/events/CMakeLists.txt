# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

find_package(PebbleOS REQUIRED HINTS $ENV{PEBBLEOS_BASE})
project(input_events)

target_sources(app PRIVATE src/main.c)
