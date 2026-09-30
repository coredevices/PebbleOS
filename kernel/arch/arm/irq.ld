/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .pbl_irq_prio : ALIGN(4) {
        __pbl_irq_prio_start = .;
        KEEP(*(.pbl_irq_prio))
        __pbl_irq_prio_end = .;
    } >FLASH
