/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .isr_vector : {
        ASSERT(. == ORIGIN(FLASH), "Error: Vector table is offset from the start of the FLASH region");
        PROVIDE(__ISR_VECTOR_TABLE__ = .);
        KEEP(*(.isr_vector))
    } >FLASH
