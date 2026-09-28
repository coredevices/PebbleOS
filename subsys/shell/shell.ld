/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .pbl_shell_root_cmds : ALIGN(4) {
        __pbl_shell_root_cmds_start = .;
        KEEP(*(SORT_BY_NAME(.pbl_shell_root_cmds.*)))
        __pbl_shell_root_cmds_end = .;
    } >FLASH

    .pbl_shell_subcmds : ALIGN(4) {
        KEEP(*(SORT_BY_NAME(.pbl_shell_subcmds.*)))
    } >FLASH
