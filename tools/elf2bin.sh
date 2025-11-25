#!/bin/bash
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


USAGE="Usage: $0 ELF_FILE"

if [ "$#" == "0" ]; then
    echo "$USAGE"
    exit 1
fi

IN_FILE=$1
IN_FILE_BASE=$(basename "$IN_FILE")
MODE=binary
OUT_FILE=$(pwd)/$(echo "$IN_FILE_BASE" | sed 's/elf/bin/')

arm-none-eabi-objcopy -O $MODE $IN_FILE $OUT_FILE
