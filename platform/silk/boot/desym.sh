#!/bin/sh
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

arm-none-eabi-addr2line --exe=build/tintin_boot.elf $1
