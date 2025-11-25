# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from .stm32 import STM32FlashProgrammer

class STM32F4FlashProgrammer(STM32FlashProgrammer):
    IDCODE = 0x2BA01477
    CPUID_VALUE = 0x410FC241

    FLASH_SECTOR_SIZES = [16, 16, 16, 16, 64, 128, 128, 128, 128, 128, 128, 128]
