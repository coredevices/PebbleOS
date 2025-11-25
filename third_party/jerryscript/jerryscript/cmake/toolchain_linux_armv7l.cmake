# SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7l)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
#
# Limit fpu to VFPv3 with d0-d15 registers
#
# If this is changed, setjmp / longjmp for ARMv7 should be updated accordingly
#
set(FLAGS_COMMON_ARCH -mthumb -mfpu=vfp -march=armv7)
