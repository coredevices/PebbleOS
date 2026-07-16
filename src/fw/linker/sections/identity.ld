/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    /* GNU build id: This is a hash of parts of the binary that uniquely
     * identifies the binary. This hash gets inserted by the linker;
     * we're passing the flag --build-id=sha1 to do this.
     * The variable TINTIN_BUILD_ID is provided, so that the values can be used
     * in the firmware code. See src/core/util/version.c for its use. */
    .note.gnu.build-id : {
        __start_gnu_build_id_start = .;
        PROVIDE(TINTIN_BUILD_ID = .);
        KEEP(*(.note.gnu.build-id))
    } >FLASH

    /* Struct with version information of the firmware. This must be the last thing that goes
     * into FLASH that has a non-zero size. */
    .fw_version : {
        KEEP(*(.pbl_fw_version))
    } >FLASH
