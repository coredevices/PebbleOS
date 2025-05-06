#include <boot_tests.h>
#include <drivers/button.h>
#include <drivers/dbgserial.h>
#include <drivers/display.h>
#include <drivers/flash.h>
#include <drivers/pmic.h>
#include <drivers/watchdog.h>
#include <firmware.h>
#include <fw_copy.h>
#include <pebble_errors.h>
#include <string.h>
#include <system/bootbits.h>
#include <system/logging.h>
#include <system/passert.h>
#include <system/reset.h>
#include <util/delay.h>
#include <util/misc.h>

static const uint8_t SELECT_BUTTON_MASK = 0x4;

static void prv_get_fw_reset_vector(void **reset_handler, void **initial_stack_pointer) {
  uintptr_t **fw_vector_table = (uintptr_t **)FIRMWARE_BASE;  // Defined in wscript
  *initial_stack_pointer = (void *)fw_vector_table[0];
  *reset_handler = (void *)fw_vector_table[1];
}

static void __attribute__((noreturn)) jump_to_fw(void) {
  void *initial_stack_pointer, *reset_handler;
  prv_get_fw_reset_vector(&reset_handler, &initial_stack_pointer);

  dbgserial_print("Booting firmware @ ");
  dbgserial_print_hex((uintptr_t)reset_handler);
  dbgserial_print("...\r\n\r\n");

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
  __asm volatile(
      "cpsie if\n"  // Clear PRIMASK and FAULTMASK
      "mov  lr, 0xFFFFFFFF\n"
      "mov  sp, %[initial_sp]\n"
      "bx   %[reset_handler]\n"
      :
      : [initial_sp] "r"(initial_stack_pointer), [reset_handler] "r"(reset_handler));
  __builtin_unreachable();
}

static bool check_and_increment_reset_loop_detection_bits(void) {
  uint8_t counter = (boot_bit_test(BOOT_BIT_RESET_LOOP_DETECT_THREE) << 2) |
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

static bool check_for_recovery_start_failure() {
  return boot_bit_test(BOOT_BIT_RECOVERY_START_IN_PROGRESS);
}

static bool check_for_fw_start_failure() {
  bool watchdog_reset = watchdog_check_clear_reset_flag();

  // Add more failure conditions here.
  if (!watchdog_reset && !boot_bit_test(BOOT_BIT_SOFTWARE_FAILURE_OCCURRED)) {
    // We're good, we're just starting normally.
    PBL_LOG_VERBOSE("We're good, we're just starting normally.");

    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_ONE);
    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_TWO);
    return false;
  }

  // We failed to start our firmware successfully!
  if (watchdog_reset) {
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
  return (button_is_pressed(BUTTON_ID_UP) && button_is_pressed(BUTTON_ID_BACK) &&
          button_is_pressed(BUTTON_ID_SELECT) && !button_is_pressed(BUTTON_ID_DOWN));
}

static bool check_force_boot_recovery(void) {
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
  if ((uintptr_t)reset_vector == 0xffffffff || (uintptr_t)initial_sp == 0xffffffff) {
    dbgserial_putstr("Firmware is erased");
    return true;
  }
  return false;
}

static void sad_watch(uint32_t error_code) {
  dbgserial_putstr("SAD WATCH");

  char error_code_buffer[12];
  itoa_hex(error_code, error_code_buffer, sizeof(error_code_buffer));
  dbgserial_putstr(error_code_buffer);

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

int main(void) {
  int ret;

  watchdog_kick();

  dbgserial_init();

  dbgserial_putstr("");
  dbgserial_putstr("   _       _           _     ");
  dbgserial_putstr("  /_\\   __| |_ ___ _ _(_)_ __");
  dbgserial_putstr(" / _ \\ (_-<  _/ -_) '_| \\ \\ /");
  dbgserial_putstr("/_/ \\_\\/__/\\__\\___|_| |_/_\\_\\");
  dbgserial_putstr("");

  boot_bit_init();

  dbgserial_putstr("boot bit");

  boot_version_write();

  // Write the bootloader version to serial-out
  {
    char bootloader_version_str[12];
    memset(bootloader_version_str, 0, 12);
    itoa_hex(boot_version_read(), bootloader_version_str, 12);
    dbgserial_putstr(bootloader_version_str);
  }
  dbgserial_putstr("");
  dbgserial_putstr("");

  if (boot_bit_test(BOOT_BIT_FW_STABLE)) {
    dbgserial_putstr("Last firmware boot was stable; clear strikes");

    boot_bit_clear(BOOT_BIT_FW_STABLE);

    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_ONE);
    boot_bit_clear(BOOT_BIT_FW_START_FAIL_STRIKE_TWO);
    boot_bit_clear(BOOT_BIT_RECOVERY_LOAD_FAIL_STRIKE_ONE);
    boot_bit_clear(BOOT_BIT_RECOVERY_LOAD_FAIL_STRIKE_TWO);
  }

  ret = pmic_init();
  if (ret != 0) {
    dbgserial_putstr("PMIC init failed");
    sad_watch(ERROR_PMIC_INIT);
  }

  flash_init();
  button_init();
  display_init();
  display_boot_splash();

#ifdef DISPLAY_DEMO_LOOP
  while (1) {
    for (int i = 0; i < 92; ++i) {
      display_firmware_update_progress(i, 91);
      delay_us(80000);
    }

    for (uint32_t i = 0; i <= 0xf; ++i) {
      display_error_code(i * 0x11111111);
      delay_us(200000);
    }
    for (uint32_t i = 0; i < 8; ++i) {
      for (uint32_t j = 1; j <= 0xf; ++j) {
        display_error_code(j << (i * 4));
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
    display_boot_splash();
    delay_us(1000000);
  }
#endif

  if (is_button_stuck()) {
    dbgserial_putstr("Stuck button");
    sad_watch(ERROR_STUCK_BUTTON);
  }

  if (is_flash_broken()) {
    dbgserial_putstr("Broken flash");
    sad_watch(ERROR_BAD_SPI_FLASH);
  }

  boot_bit_dump();

  // If the recovery firmware crashed at start-up, the watch is now a
  // $150 brick. That's life!
  if (check_for_recovery_start_failure()) {
    boot_bit_clear(BOOT_BIT_RECOVERY_START_IN_PROGRESS);
    sad_watch(ERROR_CANT_LOAD_FW);
  }

  bool force_boot_recovery_mode = check_force_boot_recovery();
  if (force_boot_recovery_mode) {
    dbgserial_putstr("Force-booting recovery mode...");
  }

  if (force_boot_recovery_mode || check_for_fw_start_failure()) {
    if (!switch_to_recovery_fw()) {
      // We've failed to load recovery mode too many times.
      sad_watch(ERROR_CANT_LOAD_FW);
    }
  } else {
    check_update_fw();
  }

  if (check_and_increment_reset_loop_detection_bits()) {
    sad_watch(ERROR_RESET_LOOP);
  }

  display_deinit();

#ifndef NO_WATCHDOG
  watchdog_init();
#endif

  jump_to_fw();

  return 0;
}

// Stubs for libg_s.a, which is our libc implementation from nano-newlib
void _exit(int status) {}
