#!/bin/bash
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


set -o errexit -o xtrace

cd "${0%/*}"

reloutdir="../../../bin/boot"
OUTDIR=`cd "${reloutdir}"; pwd`
BOARDS=(bb2 ev2_4 v1_5 v2_0)
# Use commit timestamp, same as the one compiled into the bootloader binary
VERSION=`git log -1 --format=%ct HEAD`

# Clear out old versions of the bootloader binaries
for board in ${BOARDS[*]}; do
  git rm --ignore-unmatch ${OUTDIR}/{nowatchdog_,}boot_${board}@*.{hex,elf,bin} || true
done

# Build all bootloader variants and copy them into OUTDIR
build_and_copy () {
  local variant="$1";
  shift;

  for board in ${BOARDS[*]}; do
    ./waf configure --board=${board} $@ build --progress
    for ext in hex elf bin; do
      mv build/tintin_boot.${ext} \
        "${OUTDIR}/${variant}boot_${board}@${VERSION}.${ext}"
    done
  done
}

build_and_copy ""
build_and_copy nowatchdog_ --nowatchdog
