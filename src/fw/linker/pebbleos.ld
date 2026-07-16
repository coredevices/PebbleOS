/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 *
 * Master firmware linker script. Preprocessed with the C preprocessor, so
 * Kconfig CONFIG_* symbols, #include and #if are all available.
 *
 * The FLASH, KERNEL_RAM, APP_RAM and WORKER_RAM memory regions are derived
 * from the Kconfig memory layout symbols in memory.ld. The APP_RAM and
 * WORKER_RAM regions are special: the firmware will overwrite the entire
 * regions' contents at runtime without consideration for any data or
 * variables that have been statically linked into those regions. Therefore
 * no sections may be assigned to these regions, and these regions must not
 * alias each other.
 *
 * SoC- and library-specific fragments are pulled in through the generated
 * snippets-<location>.ld files. The 'memory' hook takes top-level commands
 * ahead of the main SECTIONS block, e.g. extra MEMORY regions and their
 * sections; 'rom-start' and 'ram-sections' take output sections placed
 * right after the vector table and ahead of the kernel RAM sections
 * respectively; 'footer' takes standalone top-level commands after the
 * main SECTIONS block.
 */

#include "memory.ld"

#include <snippets-memory.ld>

#include "regions.ld"

/* Section concepts!

VMA = Virtual Memory Address - The final address for the section.
      This could be either RAM or FLASH.
LMA = Load Memory Address - The initial address in ROM that the section is loaded from.
      This is always a location in FLASH.

We lay out our LMAs like the following (some sections are skipped because they're not important)...
 .text
 .kernel_data
 .app_data
 .fw_version
    This must be last since we read it out of SPI flash before we load the rest of the image.

We take that image and load part of it into RAM. That lay out looks like...
 .kernel_data
 .kernel_bss (A small portion of which is read only from unprivileged code)
 stacks (not all of our stacks are fixed, some are on the heap and some are statically located here.
 <gap for our heap>
 .pbl_app, fixed at 0x801a000
   .app_data
   .app_bss
   .app_stack
   <gap for loaded 3rd party app code+data+bss>
   <gap for app heap>
*/

SECTIONS {
#include "sections/vectors.ld"

#include <snippets-rom-start.ld>

#include <snippets-ram-sections.ld>

#include "sections/text.ld"

#include "sections/kernel-ram.ld"

#include "sections/identity.ld"

#include "sections/log-strings.ld"

#include "sections/debug.ld"
}

/* Standalone top-level commands (e.g. whole SECTIONS blocks). */
#include <snippets-footer.ld>
