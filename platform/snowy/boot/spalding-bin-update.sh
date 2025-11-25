#!/bin/bash
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


set -o errexit -o xtrace

cd "${0%/*}"

reloutdir="../../../bin/boot"
OUTDIR=`cd "${reloutdir}"; pwd`
# Use commit timestamp, same as the one compiled into the bootloader binary
VERSION=`git log -1 --format=%ct HEAD`

# Clear out old versions of the bootloader binaries
git rm ${OUTDIR}/{nowatchdog_,}boot_spalding@*.{hex,elf} || true

# Build all bootloader variants and copy them into OUTDIR
build_and_copy () {
  local variant="$1";
  shift;

  pypy ./waf configure --board=spalding $@ build --progress
  for ext in hex elf; do
    mv build/snowy_boot.${ext} \
      "${OUTDIR}/${variant}boot_spalding@${VERSION}.${ext}"
  done
}

build_and_copy ""
build_and_copy nowatchdog_ --nowatchdog
