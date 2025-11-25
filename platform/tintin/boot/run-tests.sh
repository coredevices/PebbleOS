#!/bin/bash
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

set -o errexit

cd "${0%/*}"

CLAR_DIR=`cd ../../../tools/clar; pwd`

clar () {
  local test_name=${1:?}; shift;
  local test_suite=${1:?}; shift;
  local test_dir="build/test/${test_name}"

  mkdir -p "${test_dir}"

  python "${CLAR_DIR}/clar.py" \
      --file="${test_suite}" \
      --clar-path="${CLAR_DIR}" \
      "${test_dir}"

  gcc -o "${test_dir}/do_test" \
    -I"${test_dir}" -Isrc \
    -Ivendor/STM32F2xx_StdPeriph_Lib_V1.1.0/Libraries/CMSIS/Include \
    -Ivendor/STM32F2xx_StdPeriph_Lib_V1.1.0/Libraries/CMSIS/Device/ST/STM32F2xx/Include \
    -Ivendor/STM32F2xx_StdPeriph_Lib_V1.1.0/Libraries/STM32F2xx_StdPeriph_Driver/inc \
    -DMICRO_FAMILY_STM32F2 \
    -ffunction-sections \
    -Wl,-dead_strip \
    "${test_dir}/clar_main.c" "${test_suite}" $@
  # If running on a platform with GNU ld,
  # replace -Wl,-dead_strip with -Wl,--gc-sections

  echo "Running test ${test_suite}..."
  "${test_dir}/do_test"
}

clar system_flash_algo test/test_system_flash.c \
    src/drivers/stm32_common/system_flash.c
