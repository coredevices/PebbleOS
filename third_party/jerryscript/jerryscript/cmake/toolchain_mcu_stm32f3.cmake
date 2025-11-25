# SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

include(CMakeForceCompiler)

set(CMAKE_SYSTEM_NAME MCU)
set(CMAKE_SYSTEM_PROCESSOR armv7l)
set(CMAKE_SYSTEM_VERSION STM32F3)

set(FLAGS_COMMON_ARCH -mlittle-endian -mthumb -mcpu=cortex-m4 -march=armv7e-m -mfpu=fpv4-sp-d16 -mfloat-abi=hard)

CMAKE_FORCE_C_COMPILER(arm-none-eabi-gcc GNU)
