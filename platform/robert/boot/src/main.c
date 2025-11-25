/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include "board/board.h"
#include "drivers/button.h"
#include "drivers/dbgserial.h"
#include "drivers/display.h"
#include "drivers/flash.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/pmic.h"
#include "drivers/pwr.h"
#include "drivers/watchdog.h"
#include "boot_tests.h"
#include "firmware.h"
#include "fw_copy.h"
#include "pebble_errors.h"
#include "system/bootbits.h"
#include "system/logging.h"
#include "system/passert.h"
#include "system/reset.h"
#include "util/delay.h"
#include "util/misc.h"

#include "stm32f7xx.h"

static void prv_get_fw_reset_vector(void **reset_handler,
                                    void **initial_stack_pointer) {
  void** fw_vector_table = (void**) FIRMWARE_BASE;

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
  RCC->AHB1ENR = 0x00100000;
  RCC->AHB2ENR = 0;
  RCC->AHB3ENR = 0;
  RCC->APB1ENR = 0x00000400;  // Reserved bit needs to be set to enable RTC!
  RCC->APB2ENR = 0;

  // Reset most peripherals used by the bootloader. We want to minimize the
  // chances that the firmware unintentionally relies on some state that the
  // bootloader leaves behind. This includes disabling the PLL.
  // GPIOs are not reset here: resetting them would change their output values,
  // which could unintentionally turn of e.g. PMIC power rails.
  // The backup domain is not reset; that would be foolish.
  const uint32_t ahb1_periphs =
    RCC_AHB1Periph_CRC | RCC_AHB1Periph_DMA1 | RCC_AHB1Periph_DMA2
    | RCC_AHB1Periph_DMA2D | RCC_AHB1Periph_ETHMAC | RCC_AHB1Periph_OTGHS;
  const uint32_t ahb2_periphs =
    RCC_AHB2Periph_DCMI | RCC_AHB2Periph_JPEG | RCC_AHB2Periph_CRYP | RCC_AHB2Periph_HASH
    | RCC_AHB2Periph_RNG | RCC_AHB2Periph_OTGFS;
  const uint32_t ahb3_periphs = RCC_AHB3Periph_FMC | RCC_AHB3Periph_QSPI;
  const uint32_t apb1_periphs =
    RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3 | RCC_APB1Periph_TIM4| RCC_APB1Periph_TIM5
    | RCC_APB1Periph_TIM6 | RCC_APB1Periph_TIM7 | RCC_APB1Periph_TIM12 | RCC_APB1Periph_TIM13
    | RCC_APB1Periph_TIM14 | RCC_APB1Periph_LPTIM1 | RCC_APB1Periph_WWDG | RCC_APB1Periph_CAN3
    | RCC_APB1Periph_SPI2 | RCC_APB1Periph_SPI3 | RCC_APB1Periph_SPDIFRX | RCC_APB1Periph_USART2
    | RCC_APB1Periph_USART3 | RCC_APB1Periph_UART4 | RCC_APB1Periph_UART5 | RCC_APB1Periph_I2C1
    | RCC_APB1Periph_I2C2 | RCC_APB1Periph_I2C3 | RCC_APB1Periph_I2C4 | RCC_APB1Periph_CAN1
    | RCC_APB1Periph_CAN2 | RCC_APB1Periph_CEC | RCC_APB1Periph_PWR | RCC_APB1Periph_DAC
    | RCC_APB1Periph_UART7 | RCC_APB1Periph_UART8;
  const uint32_t apb2_periphs =
    RCC_APB2Periph_TIM1 | RCC_APB2Periph_TIM8 | RCC_APB2Periph_USART1
    | RCC_APB2Periph_USART6 | RCC_APB2Periph_SDMMC2 | RCC_APB2Periph_ADC | RCC_APB2Periph_SDMMC1
    | RCC_APB2Periph_SPI1 | RCC_APB2Periph_SPI4 | RCC_APB2Periph_SYSCFG
    | RCC_APB2Periph_TIM9 | RCC_APB2Periph_TIM10 | RCC_APB2Periph_TIM11
    | RCC_APB2Periph_SPI5 | RCC_APB2Periph_SPI6 | RCC_APB2Periph_SAI1 | RCC_APB2Periph_SAI2
    | RCC_APB2Periph_DFSDM | RCC_APB2Periph_MDIO | RCC_APB2Periph_LTDC;
  RCC_DeInit();
  RCC_AHB1PeriphResetCmd(ahb1_periphs, ENABLE);
  RCC_AHB1PeriphResetCmd(ahb1_periphs, DISABLE);
  RCC_AHB2PeriphResetCmd(ahb2_periphs, ENABLE);
  RCC_AHB2PeriphResetCmd(ahb2_periphs, DISABLE);
  RCC_AHB3PeriphResetCmd(ahb3_periphs, ENABLE);
  RCC_AHB3PeriphResetCmd(ahb3_periphs, DISABLE);
  RCC_APB1PeriphResetCmd(apb1_periphs, ENABLE);
  RCC_APB1PeriphResetCmd(apb1_periphs, DISABLE);
  RCC_APB2PeriphResetCmd(apb2_periphs, ENABLE);
  RCC_APB2PeriphResetCmd(apb2_periphs, DISABLE);
}

static void __attribute__((noreturn)) prv_jump_to_fw(void) {
  void *initial_stack_pointer, *reset_handler;
  prv_get_fw_reset_vector(&reset_handler, &initial_stack_pointer);

  dbgserial_print("Booting firmware @ ");
  dbgserial_print_hex((uintptr_t)reset_handler);
  dbgserial_print("...\r\n\r\n");

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

  if (counter == 7) {
    boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_ONE);
    boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_TWO);
    boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_THREE);
    return true;
  }

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
    PBL_CROAK("reset loop boot bits overrun");
    break;
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
    PBL_LOG_VERBOSE("We're good, we're just starting normally.");

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
    dbgserial_putstr("Failed to start firmware, strike three.");
    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_ONE);
    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_TWO);
    return true;
  } else if (boot_bit_test(BOOT_BIT_FW_START_FAIL_STRIKE_ONE)) {
    dbgserial_putstr("Failed to start firmware, strike two.");
    boot_bit_set(BOOT_BIT_FW_START_FAIL_STRIKE_TWO);
  } else {
    dbgserial_putstr("Failed to start firmware, strike one.");
    boot_bit_set(BOOT_BIT_FW_START_FAIL_STRIKE_ONE);
  }

  return false;
}

static bool prv_prf_button_combination_is_pressed(void) {
  return (button_is_pressed(BUTTON_ID_UP) && button_is_pressed(BUTTON_ID_BACK)
      && button_is_pressed(BUTTON_ID_SELECT) && !button_is_pressed(BUTTON_ID_DOWN));
}

static bool prv_check_force_boot_recovery(void) {
  if (boot_bit_test(BOOT_BIT_FORCE_PRF)) {
    boot_bit_clear(BOOT_BIT_FORCE_PRF);
    return true;
  }

  if (prv_prf_button_combination_is_pressed()) {
    dbgserial_putstr("Hold down UP + BACK + SELECT for 5 secs. to force-boot PRF");
    for (int i = 0; i < 5000; ++i) {
      if (!prv_prf_button_combination_is_pressed()) {
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
  dbgserial_putstr("SAD WATCH");

  char error_code_buffer[12];
  itoa_hex(error_code, error_code_buffer, sizeof(error_code_buffer));
  dbgserial_putstr(error_code_buffer);

  display_error_code(error_code);

  bool prev_select_state = button_is_pressed(BUTTON_ID_SELECT);
  while (1) {
    // See if we should restart
    bool select_state = button_is_pressed(BUTTON_ID_SELECT);
    if (select_state != prev_select_state) {
      system_reset();
    }
    delay_ms(10);
  }
}

static void prv_check_and_handle_resuming_from_standby(void) {
  periph_config_enable(PWR, RCC_APB1Periph_PWR);
  if (pwr_did_boot_from_standby()) {
    // We just woke up from standby. For some reason this leaves the system in a funny state,
    // so clear the flag and reboot again to really clear things up.

    pwr_clear_boot_from_standby_flag();
    dbgserial_putstr("exit standby");
    system_hard_reset();
  }
  periph_config_disable(PWR, RCC_APB1Periph_PWR);
}

static void prv_print_bootloader_version(void) {
  char bootloader_version_str[12];
  memset(bootloader_version_str, 0, 12);
  itoa_hex(boot_version_read(), bootloader_version_str, 12);
  dbgserial_putstr(bootloader_version_str);
  dbgserial_putstr("");
  dbgserial_putstr("");
}

int main(void) {
  prv_check_and_handle_resuming_from_standby();

  board_init();

  dbgserial_init();

  dbgserial_putstr("\r\n\r\n\r\n");
  dbgserial_putstr("██████╗  ██████╗ ██████╗ ███████╗██████╗ ████████╗");
  dbgserial_putstr("██╔══██╗██╔═══██╗██╔══██╗██╔════╝██╔══██╗╚══██╔══╝");
  dbgserial_putstr("██████╔╝██║   ██║██████╔╝█████╗  ██████╔╝   ██║   ");
  dbgserial_putstr("██╔══██╗██║   ██║██╔══██╗██╔══╝  ██╔══██╗   ██║   ");
  dbgserial_putstr("██║  ██║╚██████╔╝██████╔╝███████╗██║  ██║   ██║   ");
  dbgserial_putstr("╚═╝  ╚═╝ ╚═════╝ ╚═════╝ ╚══════╝╚═╝  ╚═╝   ╚═╝   ");

  // Enable the 3.2V rail for the benefit of the FPGA and display
  pmic_init();

  boot_bit_init();

  boot_version_write();

  prv_print_bootloader_version();

  if (boot_bit_test(BOOT_BIT_FW_STABLE)) {
    dbgserial_putstr("Last firmware boot was stable; clear strikes");

    boot_bit_clear(BOOT_BIT_FW_STABLE);

    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_ONE);
    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_TWO);
    boot_bit_clear(BOOT_BIT_RECOVERY_LOAD_FAIL_STRIKE_ONE);
    boot_bit_clear(BOOT_BIT_RECOVERY_LOAD_FAIL_STRIKE_TWO);
  }

  flash_init();
  button_init();
  pmic_init();
  display_init();

  display_boot_splash();

  if (boot_test_is_button_stuck()) {
    prv_sad_watch(ERROR_STUCK_BUTTON);
  }

  if (boot_test_is_flash_broken()) {
    prv_sad_watch(ERROR_BAD_SPI_FLASH);
  }

  boot_bit_dump();

  // If the recovery firmware crashed at start-up, the watch is now a $199 brick. That's life!
  if (prv_check_for_recovery_start_failure()) {
    boot_bit_clear(BOOT_BIT_RECOVERY_START_IN_PROGRESS);
    prv_sad_watch(ERROR_CANT_LOAD_FW);
  }

  bool force_boot_recovery_mode = prv_check_force_boot_recovery();
  if (force_boot_recovery_mode) {
    dbgserial_putstr("Force-booting recovery mode...");
  }

  if (force_boot_recovery_mode || prv_check_for_fw_start_failure()) {
    if (!fw_copy_switch_to_recovery_fw()) {
      // We've failed to load recovery mode too many times.
      prv_sad_watch(ERROR_CANT_LOAD_FW);
    }
  } else {
    fw_copy_check_update_fw();
  }

  if (prv_check_and_increment_reset_loop_detection_bits()) {
    prv_sad_watch(ERROR_RESET_LOOP);
  }

#if !NO_WATCHDOG
  dbgserial_putstr("Enabling watchdog");
  watchdog_init();
  watchdog_start();
#endif

  gpio_disable_all();

  prv_jump_to_fw();
}

// Stubs for libg_s.a, which is our libc implementation from nano-newlib
void _exit(int status) {
}
