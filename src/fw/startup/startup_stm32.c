/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! Initial firmware startup, contains the vector table that the bootloader loads.
//! Based on "https://github.com/pfalcon/cortex-uni-startup/blob/master/startup.c"
//! by Paul Sokolovsky (public domain)

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "mcu/cache.h"
#include "util/attributes.h"

//! These symbols are defined in the linker script for use in initializing
//! the data sections. uint8_t since we do arithmetic with section lengths.
//! These are arrays to avoid the need for an & when dealing with linker symbols.
extern uint8_t __data_load_start[];
extern uint8_t __data_start[];
extern uint8_t __data_end[];
extern uint8_t __bss_start[];
extern uint8_t __bss_end[];
extern uint8_t _estack[];

#if MICRO_FAMILY_STM32F7
extern uint8_t __dtcm_bss_start[];
extern uint8_t __dtcm_bss_end[];
#endif

//! Firmware main function, ResetHandler calls this
extern int main(void);

//! STM32 system initialization function, defined in the standard peripheral library
extern void SystemInit(void);

//! This function is what gets called when the processor first
//! starts execution following a reset event. The data and bss
//! sections are initialized, then we call the firmware's main
//! function
NORETURN Reset_Handler(void) {
  // Copy data section from flash to RAM
  memcpy(__data_start, __data_load_start, __data_end - __data_start);

  // Clear the bss section, assumes .bss goes directly after .data
  memset(__bss_start, 0, __bss_end - __bss_start);

#if MICRO_FAMILY_STM32F7
  // Clear the DTCM bss section
  memset(__dtcm_bss_start, 0, __dtcm_bss_end - __dtcm_bss_start);
#endif

  SystemInit();

  icache_enable();
  dcache_enable();

  main();

  // Main shouldn't return
  while (true) {}
}
