# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from .stm32 import STM32FlashProgrammer

class STM32F7FlashProgrammer(STM32FlashProgrammer):
    IDCODE = 0x5BA02477
    CPUID_VALUE = 0x411FC270

    FLASH_SECTOR_SIZES = [32, 32, 32, 32, 128, 256, 256, 256, 256, 256, 256, 256]
