# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

find_package(PebbleOS REQUIRED HINTS $ENV{PEBBLEOS_BASE})
project(ios_clients)

target_sources(app PRIVATE src/main.c)

if(CONFIG_SAMPLE_ANCS)
  target_sources(app PRIVATE src/ancs.c)
endif()

if(CONFIG_SAMPLE_AMS)
  target_sources(app PRIVATE src/ams.c)
endif()
