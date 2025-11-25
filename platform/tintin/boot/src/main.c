/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <inttypes.h>

#include "board/board.h"
#include "boot_tests.h"
#include "drivers/button.h"
#include "drivers/dbgserial.h"
#include "drivers/display.h"
#include "drivers/flash.h"
#include "drivers/periph_config.h"
#include "drivers/rtc.h"
#include "drivers/watchdog.h"
#include "firmware.h"
#include "fw_copy.h"
#include "pebble_errors.h"
#include "standby.h"
#include "system/bootbits.h"
#include "system/reset.h"
#include "util/delay.h"

#include "stm32f2xx.h"

static const uint8_t SELECT_BUTTON_MASK = 0x4;

bool firmware_is_new_world(void* base) {
  uint32_t* fw_base = base;
  if (fw_base == NULL) {
    fw_base = (uint32_t*)FIRMWARE_NEWWORLD_BASE;
  }
  if (fw_base[0] == 0xFFFFFFFF ||
      fw_base[1] == 0xFFFFFFFF) { // Erased flash
    return false;
  }
  if (fw_base[FW_IDENTIFIER_OFFSET / sizeof(uint32_t)] != 0x4E65576F) { // NeWo
    return false;
  }
  // TODO: Additional checks?
  return true;
}

static void prv_get_fw_reset_vector(void **reset_handler,
                                 void **initial_stack_pointer) {
  void** fw_vector_table;
  if (firmware_is_new_world(NULL)) {
    fw_vector_table = (void**) FIRMWARE_NEWWORLD_BASE;
  } else {
    fw_vector_table = (void**) FIRMWARE_OLDWORLD_BASE;
  }
  *initial_stack_pointer = fw_vector_table[0];
  *reset_handler = fw_vector_table[1];
}

static void prv_hw_reset(void) {
  // Disable all interrupts, just in case.
  for (int i = 0; i < 8; ++i) {
    // Interrupt Clear-Enable Register
    NVIC->ICER[i] = 0xFFFFFFFF;
    // Interrupt Clear-Pending Register
    NVIC->ICPR[i] = 0xFFFFFFFF;
  }

  // Set the peripheral clock enable registers to their reset values as
  // specified in the reference manual.
  RCC->AHB1ENR = 0;
  RCC->AHB2ENR = 0;
  RCC->AHB3ENR = 0;
  RCC->APB1ENR = 0;
  RCC->APB2ENR = 0;

  // Reset most peripherals used by the bootloader. We want to minimize the
  // chances that the firmware unintentionally relies on some state that the
  // bootloader leaves behind. This includes disabling the PLL.
  // GPIOs are not reset here: resetting them would change their output values,
  // which could unintentionally modify peripherals (such as the display).
  // The backup domain is not reset; that would be foolish.
  RCC_DeInit();

  // Reset flags for each bus taken from reset register lists in reference manual
  // starting with 5.3.5 "RCC AHB1 peripheral reset register"

  const uint32_t ahb1_periphs = 0
    | RCC_AHB1Periph_CRC
    | RCC_AHB1Periph_DMA1
    | RCC_AHB1Periph_DMA2
    | RCC_AHB1Periph_ETH_MAC
    | RCC_AHB1Periph_OTG_HS;

  RCC_AHB1PeriphResetCmd(ahb1_periphs, ENABLE);
  RCC_AHB1PeriphResetCmd(ahb1_periphs, DISABLE);

  const uint32_t ahb2_periphs = 0
    | RCC_AHB2Periph_DCMI
    | RCC_AHB2Periph_CRYP
    | RCC_AHB2Periph_HASH
    | RCC_AHB2Periph_RNG
    | RCC_AHB2Periph_OTG_FS;

  RCC_AHB2PeriphResetCmd(ahb2_periphs, ENABLE);
  RCC_AHB2PeriphResetCmd(ahb2_periphs, DISABLE);

  const uint32_t ahb3_periphs = 0
    | RCC_AHB3Periph_FSMC;

  RCC_AHB3PeriphResetCmd(ahb3_periphs, ENABLE);
  RCC_AHB3PeriphResetCmd(ahb3_periphs, DISABLE);

  const uint32_t apb1_periphs = 0
    | RCC_APB1Periph_TIM2
    | RCC_APB1Periph_TIM3
    | RCC_APB1Periph_TIM4
    | RCC_APB1Periph_TIM5
    | RCC_APB1Periph_TIM6
    | RCC_APB1Periph_TIM7
    | RCC_APB1Periph_TIM12
    | RCC_APB1Periph_TIM13
    | RCC_APB1Periph_TIM14
    | RCC_APB1Periph_WWDG
    | RCC_APB1Periph_SPI2
    | RCC_APB1Periph_SPI3
    | RCC_APB1Periph_USART2
    | RCC_APB1Periph_USART3
    | RCC_APB1Periph_UART4
    | RCC_APB1Periph_UART5
    | RCC_APB1Periph_I2C1
    | RCC_APB1Periph_I2C2
    | RCC_APB1Periph_I2C3
    | RCC_APB1Periph_CAN1
    | RCC_APB1Periph_CAN2
    | RCC_APB1Periph_PWR
    | RCC_APB1Periph_DAC;

  RCC_APB1PeriphResetCmd(apb1_periphs, ENABLE);
  RCC_APB1PeriphResetCmd(apb1_periphs, DISABLE);

  const uint32_t apb2_periphs = 0
    | RCC_APB2Periph_TIM1
    | RCC_APB2Periph_TIM8
    | RCC_APB2Periph_USART1
    | RCC_APB2Periph_USART6
    | RCC_APB2Periph_ADC
    | RCC_APB2Periph_SDIO
    | RCC_APB2Periph_SPI1
    | RCC_APB2Periph_SYSCFG
    | RCC_APB2Periph_TIM9
    | RCC_APB2Periph_TIM10
    | RCC_APB2Periph_TIM11;

  RCC_APB2PeriphResetCmd(apb2_periphs, ENABLE);
  RCC_APB2PeriphResetCmd(apb2_periphs, DISABLE);
}

static void __attribute__((noreturn)) prv_jump_to_fw(void) {
  void *initial_stack_pointer, *reset_handler;
  prv_get_fw_reset_vector(&reset_handler, &initial_stack_pointer);

  dbgserial_print("Booting firmware @ ");
  dbgserial_print_hex((uintptr_t)reset_handler);
  dbgserial_newline();
  dbgserial_newline();

  prv_hw_reset();

  // The Cortex-M user guide states that the reset values for the core registers
  // are as follows:
  //   R0-R12 = Unknown
  //   MSP = VECTOR_TABLE[0]  (main stack pointer)
  //   PSP = Unknown          (process stack pointer)
  //   LR  = 0xFFFFFFFF
  //   PC  = VECTOR_TABLE[1]
  //   PRIMASK   = 0x0
  //   FAULTMASK = 0x0
  //   BASEPRI   = 0x0
  //   CONTROL   = 0x0
  //
  // Attempt to put the processor into as close to the reset state as possible
  // before passing control to the firmware.
  //
  // No attempt is made to set CONTROL to zero as it should already be set to
  // the reset value when this code executes.
  __asm volatile (
      "cpsie if\n"  // Clear PRIMASK and FAULTMASK
      "mov  lr, 0xFFFFFFFF\n"
      "mov  sp, %[initial_sp]\n"
      "bx   %[reset_handler]\n"
      : : [initial_sp] "r" (initial_stack_pointer),
          [reset_handler] "r" (reset_handler)
  );
  __builtin_unreachable();
}

static bool prv_check_and_increment_reset_loop_detection_bits(void) {
  uint8_t counter =
    (boot_bit_test(BOOT_BIT_RESET_LOOP_DETECT_THREE) << 2) |
    (boot_bit_test(BOOT_BIT_RESET_LOOP_DETECT_TWO) << 1) |
    boot_bit_test(BOOT_BIT_RESET_LOOP_DETECT_ONE);

  switch (++counter) {
  case 1:
    boot_bit_set(BOOT_BIT_RESET_LOOP_DETECT_ONE);
    break;
  case 2:
    boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_ONE);
    boot_bit_set(BOOT_BIT_RESET_LOOP_DETECT_TWO);
    break;
  case 3:
    boot_bit_set(BOOT_BIT_RESET_LOOP_DETECT_ONE);
    break;
  case 4:
    boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_ONE);
    boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_TWO);
    boot_bit_set(BOOT_BIT_RESET_LOOP_DETECT_THREE);
    break;
  case 5:
    boot_bit_set(BOOT_BIT_RESET_LOOP_DETECT_ONE);
    break;
  case 6:
    boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_ONE);
    boot_bit_set(BOOT_BIT_RESET_LOOP_DETECT_TWO);
    break;
  case 7:
    boot_bit_set(BOOT_BIT_RESET_LOOP_DETECT_ONE);
    break;
  default:
    boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_ONE);
    boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_TWO);
    boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_THREE);
    return true;
  }
  return false;
}

static bool prv_check_for_recovery_start_failure() {
  return boot_bit_test(BOOT_BIT_RECOVERY_START_IN_PROGRESS);
}

static bool prv_check_for_fw_start_failure() {
  // Add more failure conditions here.
  if (!watchdog_check_reset_flag() && !boot_bit_test(BOOT_BIT_SOFTWARE_FAILURE_OCCURRED)) {
    // We're good, we're just starting normally.
    dbgserial_putstr("Booting normally");

    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_ONE);
    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_TWO);
    return false;
  }

  // We failed to start our firmware successfully!
  if (watchdog_check_reset_flag()) {
    dbgserial_putstr("Watchdog caused a reset");
  }
  if (boot_bit_test(BOOT_BIT_SOFTWARE_FAILURE_OCCURRED)) {
    dbgserial_putstr("Software failure caused a reset");
  }

  // Clean up after the last failure.
  boot_bit_clear(BOOT_BIT_SOFTWARE_FAILURE_OCCURRED);

  // We have a "three strikes" algorithm: if the watch fails three times, return true
  // to tell the parent we should load the recovery firmware. A reset for any other reason
  // will reset this algorithm.
  if (boot_bit_test(BOOT_BIT_FW_START_FAIL_STRIKE_TWO)) {
    // Yikes, our firmware is screwed. Boot into recovery mode.
    dbgserial_putstr("Boot failed, strike 3");
    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_ONE);
    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_TWO);
    return true;
  } else if (boot_bit_test(BOOT_BIT_FW_START_FAIL_STRIKE_ONE)) {
    dbgserial_putstr("Boot failed, strike 2");
    boot_bit_set(BOOT_BIT_FW_START_FAIL_STRIKE_TWO);
  } else {
    dbgserial_putstr("Boot failed, strike 1");
    boot_bit_set(BOOT_BIT_FW_START_FAIL_STRIKE_ONE);
  }

  return false;
}

static bool prv_check_force_boot_recovery(void) {
  if (boot_bit_test(BOOT_BIT_FORCE_PRF)) {
    boot_bit_clear(BOOT_BIT_FORCE_PRF);
    return true;
  }

  if (button_is_pressed(BUTTON_ID_UP) && button_is_pressed(BUTTON_ID_BACK)) {
    dbgserial_putstr("Hold down UP + BACK for 5 secs. to force-boot PRF");
    for (int i = 0; i < 5000; ++i) {
      if (!(button_is_pressed(BUTTON_ID_UP) && button_is_pressed(BUTTON_ID_BACK))) {
        // stop waiting if not held down any longer
        return false;
      }
      delay_ms(1);
    }

    return true;
  }

  void *reset_vector, *initial_sp;
  prv_get_fw_reset_vector(&reset_vector, &initial_sp);
  if ((uintptr_t)reset_vector == 0xffffffff ||
      (uintptr_t)initial_sp == 0xffffffff) {
    dbgserial_putstr("Firmware is erased");
    return true;
  }
  return false;
}

static void prv_sad_watch(uint32_t error_code) {
  dbgserial_print("SAD WATCH: ");
  dbgserial_print_hex(error_code);
  dbgserial_newline();

  display_error_code(error_code);

  static uint8_t prev_button_state = 0;
  prev_button_state = button_get_state_bits() & ~SELECT_BUTTON_MASK;
  while (1) {
    // See if we should restart
    uint8_t button_state = button_get_state_bits() & ~SELECT_BUTTON_MASK;
    if (button_state != prev_button_state) {
      system_reset();
    }

    delay_ms(10);
  }
}

static void prv_print_reset_reason(void) {
  dbgserial_print("Reset Register ");
  dbgserial_print_hex(RCC->CSR);
  dbgserial_newline();
  if (RCC_GetFlagStatus(RCC_FLAG_BORRST) == SET) {
    dbgserial_putstr("Brown out reset");
  }
}

// SystemInit does this for the firmware, but since the bootloader isn't using
// the vendor SystemInit, initialize the flash cache here
static void prv_configure_system_flash(void) {
  // Enable flash instruction and data caches
  FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN;
}

// RTC and bootbit assume access to the backup
// registers has been enabled
static void prv_enable_backup_access() {
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
  PWR_BackupAccessCmd(ENABLE);  // Disable write-protect on RTC_BKP_x registers
}

void boot_main(void) {
  prv_configure_system_flash();
  prv_enable_backup_access();

  dbgserial_init();
  prv_print_reset_reason();

  boot_bit_init();

  if (!rtc_init()) {
    // Need to initialize the display in this
    // case so we can see the sad watch
    display_init();
    prv_sad_watch(ERROR_CANT_START_LSE);
  }

  // Standby checks need to know the button pressed state
  button_init();

  // On tintin the bootloader handles entering and leaving standby manually
  if (boot_bit_test(BOOT_BIT_STANDBY_MODE_REQUESTED)) {
    boot_bit_clear(BOOT_BIT_STANDBY_MODE_REQUESTED);
    enter_standby_mode();
  } else if (PWR_GetFlagStatus(PWR_FLAG_SB) == SET) { // Woke up from standby
    // Clear the standby flag since only a power reset clears it
    PWR_ClearFlag(PWR_FLAG_SB);

    // Before coming out of standby make sure we should be waking up
    if (should_leave_standby_mode()) {
      leave_standby_mode();
    } else {
      dbgserial_putstr("returning to standby");
      enter_standby_mode();
    }

    dbgserial_putstr("leaving standby");
  } else {
    // If not entering or leaving standby this is a cold boot. The firmware
    // expects the clock to be running in fast mode
    rtc_initialize_fast_mode();
  }

  // Print out our super cool bootloader logo:
  //  ______    __
  // /_  __/ __/ /_
  //  / /   /_  __/
  // /_/     /_/

  dbgserial_putstr(" ______    __\r\n/_  __/ __/ /\r\n / /   /_  __/\r\n/_/     /_/\r\n");

  boot_version_write();

  // Write the bootloader version to serial-out
  dbgserial_print("Bootloader version: ");
  dbgserial_print_hex(boot_version_read());
  dbgserial_newline();

  if (boot_bit_test(BOOT_BIT_FW_STABLE)) {
    dbgserial_putstr("Last firmware boot was stable; clear strikes");

    boot_bit_clear(BOOT_BIT_FW_STABLE);

    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_ONE);
    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_TWO);
    boot_bit_clear(BOOT_BIT_RECOVERY_LOAD_FAIL_STRIKE_ONE);
    boot_bit_clear(BOOT_BIT_RECOVERY_LOAD_FAIL_STRIKE_TWO);
  }

  display_init();
  display_boot_splash();

#ifdef DISPLAY_DEMO_LOOP
  while (1) {
    display_boot_splash();
    delay_us(1000000);

    for (int i=0; i <= 91; ++i) {
      display_firmware_update_progress(i, 91);
      delay_us(80000);
    }

    for (uint32_t i=0; i <= 0xf; ++i) {
      display_error_code(i * 0x11111111);
      delay_us(200000);
    }
    for (uint32_t i=0; i < 8; ++i) {
      for (uint32_t j=1; j <= 0xf; ++j) {
        display_error_code(j << (i*4));
        delay_us(200000);
      }
    }
    display_error_code(0x01234567);
    delay_us(200000);
    display_error_code(0x89abcdef);
    delay_us(200000);
    display_error_code(0xcafebabe);
    delay_us(200000);
    display_error_code(0xfeedface);
    delay_us(200000);
    display_error_code(0x8badf00d);
    delay_us(200000);
    display_error_code(0xbad1ce40);
    delay_us(200000);
    display_error_code(0xbeefcace);
    delay_us(200000);
    display_error_code(0x0defaced);
    delay_us(200000);
    display_error_code(0xd15ea5e5);
    delay_us(200000);
    display_error_code(0xdeadbeef);
    delay_us(200000);
  }
#endif

  flash_init();

  if (is_button_stuck()) {
    prv_sad_watch(ERROR_STUCK_BUTTON);
  }

  if (is_flash_broken()) {
    prv_sad_watch(ERROR_BAD_SPI_FLASH);
  }

  boot_bit_dump();

  // If the recovery firmware crashed at start-up, the watch is now a
  // $150 brick. That's life!
  if (prv_check_for_recovery_start_failure()) {
    boot_bit_clear(BOOT_BIT_RECOVERY_START_IN_PROGRESS);
    prv_sad_watch(ERROR_CANT_LOAD_FW);
  }

  bool force_boot_recovery_mode = prv_check_force_boot_recovery();
  if (force_boot_recovery_mode) {
    dbgserial_putstr("Force-booting recovery mode...");
  }

  if (force_boot_recovery_mode || prv_check_for_fw_start_failure()) {
    if (!switch_to_recovery_fw()) {
      // We've failed to load recovery mode too many times.
      prv_sad_watch(ERROR_CANT_LOAD_FW);
    }
  } else {
    check_update_fw();
  }

  if (prv_check_and_increment_reset_loop_detection_bits()) {
    prv_sad_watch(ERROR_RESET_LOOP);
  }

  watchdog_init();
#ifndef NO_WATCHDOG
  watchdog_start();
#endif

  prv_jump_to_fw();
}
