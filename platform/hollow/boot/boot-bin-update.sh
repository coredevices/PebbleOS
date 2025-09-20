#!/bin/bash
# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


set -o errexit -o xtrace

cd "${0%/*}"

reloutdir="../../../bin/boot"
OUTDIR=`cd "${reloutdir}"; pwd`
BOARDS=(asterix)
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
