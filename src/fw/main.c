/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <setjmp.h>

#include "debug/power_tracking.h"

#include "board/board.h"

#include "console/dbgserial.h"
#include "console/dbgserial_input.h"
#include "console/pulse.h"

#include "drivers/clocksource.h"
#include "drivers/rtc.h"
#include "drivers/periph_config.h"
#include "drivers/flash.h"
#include "drivers/debounced_button.h"

#include "drivers/accel.h"
#include "drivers/ambient_light.h"
#include "drivers/backlight.h"
#include "drivers/battery.h"
#include "drivers/display/display.h"
#include "drivers/gpio.h"
#include "drivers/hrm.h"
#include "drivers/led_controller.h"
#include "drivers/mag.h"
#include "drivers/mic.h"
#include "drivers/otp.h"
#include "drivers/pmic.h"
#include "drivers/pressure.h"
#include "drivers/pwr.h"
#include "drivers/spi.h"
#include "drivers/task_watchdog.h"
#include "drivers/temperature.h"
#include "drivers/touch/touch_sensor.h"
#include "drivers/vibe.h"
#include "drivers/voltage_monitor.h"
#include "drivers/watchdog.h"
#include "drivers/sf32lb52/rc10k.h"

#include "resource/resource.h"
#include "resource/system_resource.h"

#include "kernel/util/stop.h"
#include "kernel/util/task_init.h"
#include "kernel/util/sleep.h"
#include "kernel/events.h"
#include "kernel/kernel_heap.h"
#include "kernel/fault_handling.h"
#include "kernel/memory_layout.h"
#include "kernel/panic.h"
#include "kernel/pulse_logging.h"
#include "services/services.h"
#include "services/common/boot_splash.h"
#include "services/common/clock.h"
#include "services/common/compositor/compositor.h"
#include "services/common/regular_timer.h"
#include "services/common/system_task.h"
#include "services/common/new_timer/new_timer_service.h"
#include "services/common/new_timer/new_timer.h"
#include "services/common/analytics/analytics.h"
#include "services/common/prf_update.h"
#include "kernel/ui/kernel_ui.h"
#include "kernel/kernel_applib_state.h"
#include "kernel/util/delay.h"
#include "util/mbuf.h"
#include "system/firmware_storage.h"
#include "system/version.h"

#include "kernel/event_loop.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "applib/ui/ui.h"
#include "applib/ui/window_stack_private.h"

#include "console/serial_console.h"
#include "system/bootbits.h"
#include "system/logging.h"
#include "system/passert.h"
#include "system/reset.h"

#include "syscall/syscall_internal.h"

#include "debug/debug.h"
#include "debug/setup.h"

#include "FreeRTOS.h"
#include "task.h"

#include "mfg/mfg_info.h"
#include "mfg/mfg_serials.h"

#include <bluetooth/init.h>

#include <string.h>

void soc_early_init(void);

/* here is as good as anywhere else ... */
const int __attribute__((used)) uxTopUsedPriority = configMAX_PRIORITIES - 1;

static TimerID s_lowpower_timer = TIMER_INVALID_ID;
#ifndef MANUFACTURING_FW
static TimerID s_uptime_timer = TIMER_INVALID_ID;
#endif
static void main_task(void *parameter);

static void print_splash_screen(void)
{

#if defined(MANUFACTURING_FW)
  PBL_LOG_ALWAYS("PebbleOS - MANUFACTURING MODE");
#elif defined(RECOVERY_FW)
  PBL_LOG_ALWAYS("PebbleOS - RECOVERY MODE");
#else
  PBL_LOG_ALWAYS("PebbleOS");
#endif
  PBL_LOG_ALWAYS("%s%s",
          TINTIN_METADATA.version_tag,
          (TINTIN_METADATA.is_dual_slot && !TINTIN_METADATA.is_recovery_firmware) ?
            (TINTIN_METADATA.is_slot_0 ? " (slot0)" : " (slot1)") :
            "");
  PBL_LOG_ALWAYS("(c) 2013-2025 The PebbleOS contributors");
  PBL_LOG_ALWAYS(" ");
}

#ifdef DUMP_GPIO_CFG_STATE
static void dump_gpio_configuration_state(void) {
  char name[2] = { 0 };
  name[0] = 'A'; // GPIO Port A

  GPIO_TypeDef *gpio_pin;

  for (uint32_t gpio_addr = (uint32_t)GPIOA; gpio_addr <= (uint32_t)GPIOI;
       gpio_addr += 0x400) {
    gpio_pin = (GPIO_TypeDef *)gpio_addr;

    gpio_use(gpio_pin);
    uint32_t mode = gpio_pin->MODER;
    gpio_release(gpio_pin);

    uint16_t pin_cfg_mask = 0;
    char buf[80];
    for (int pin = 0; pin < 16; pin++) {
      if ((mode & GPIO_MODER_MODER0) != GPIO_Mode_AN) {
        pin_cfg_mask |= (0x1 << pin);
      }
      mode >>= 2;
    }

    dbgserial_putstr_fmt(buf, sizeof(buf), "Non Analog P%s cfg: 0x%"PRIx16,
      name, (int)pin_cfg_mask);
    name[0]++;
  }
}
#endif /* DUMP_GPIO_CFG_STATE */

int main(void) {
  soc_early_init();

#if defined(MICRO_FAMILY_STM32F4)
  gpio_init_all();
#endif

#if defined(MICRO_FAMILY_STM32F4) && !defined(LOW_POWER_DEBUG)
  // If we're on a snowy board using the stm32f4, we experience random hardfaults after leaving a
  // wfi instruction if we have mcu debugging enabled. For now, just turn off mcu debugging
  // entirely unless we explicitly want it. See PBL-10174
  disable_mcu_debugging();
#else
  // Turn on MCU debugging at boot. This consumes some power so we'll turn it off after a short
  // time has passed (see prv_low_power_debug_config_callback) to allow us to connect after a
  // reset but not passively consume power after we've been running for a bit.
  enable_mcu_debugging();
#endif

  extern void * __ISR_VECTOR_TABLE__;  // Defined in linker script
  SCB->VTOR = (uint32_t)&__ISR_VECTOR_TABLE__;

  NVIC_SetPriorityGrouping(3); // 4 bits for group priority; 0 bits for subpriority

  enable_fault_handlers();

  kernel_heap_init();

  mbuf_init();
  delay_init();
#if defined(MICRO_FAMILY_STM32F4)
  periph_config_init();
#endif
  dbgserial_init();
  pulse_early_init();
  print_splash_screen();

  rtc_init();

#ifdef RECOVERY_FW
  boot_bit_clear(BOOT_BIT_RECOVERY_START_IN_PROGRESS);
#endif

  extern uint32_t __kernel_main_stack_start__[];
  extern uint32_t __kernel_main_stack_size__[];
  extern uint32_t __stack_guard_size__[];
  const uint32_t kernel_main_stack_words = ( (uint32_t)__kernel_main_stack_size__
                            - (uint32_t) __stack_guard_size__ ) / sizeof(portSTACK_TYPE);

  TaskParameters_t task_params = {
    .pvTaskCode = main_task,
    .pcName = "KernelMain",
    .usStackDepth = kernel_main_stack_words,
    .uxPriority = (tskIDLE_PRIORITY + 3) | portPRIVILEGE_BIT,
    .puxStackBuffer = (void*)(uintptr_t)((uint32_t)__kernel_main_stack_start__
                                          + (uint32_t)__stack_guard_size__)
  };

  pebble_task_create(PebbleTask_KernelMain, &task_params, NULL);

  // Always start the firmware in a state where we explicitly do not allow stop mode.
  // FIXME: This seems overly cautious to me, we shouldn't have to do this.
  stop_mode_disable(InhibitorMain);

  // Turn off power to internal flash when in stop mode
#if MICRO_FAMILY_STM32F4
  periph_config_enable(PWR, RCC_APB1Periph_PWR);
#endif
  pwr_flash_power_down_stop_mode(true /* power_down */);
#if MICRO_FAMILY_STM32F4
  periph_config_disable(PWR, RCC_APB1Periph_PWR);
#endif

  vTaskStartScheduler();
  for(;;);
}

static void watchdog_timer_callback(void* data) {
  task_watchdog_bit_set(PebbleTask_NewTimers);
}

static void vcom_timer_callback(void* data) {
  display_pulse_vcom();
}

static void register_system_timers(void) {
  static RegularTimerInfo watchdog_timer = { .list_node = { 0, 0 }, .cb = watchdog_timer_callback };
  regular_timer_add_seconds_callback(&watchdog_timer);

  if (BOARD_CONFIG.lcd_com.gpio != 0) {
    static RegularTimerInfo vcom_timer = { .list_node = { 0, 0 }, .cb = vcom_timer_callback };
    regular_timer_add_seconds_callback(&vcom_timer);
  }
}

static void init_drivers(void) {
  board_init();

  // The dbgserial input support requires timer support, so it is initialized here, much later
  // than the core dbgserial_init().
  dbgserial_input_init();

  serial_console_init();

#ifdef HAS_DRIVER_VOLTAGE_MONITOR
  voltage_monitor_init();
#endif

  battery_init();
  vibe_init();

#ifdef CONFIG_PMIC
  pmic_init();
#endif

  flash_init();
  flash_sleep_when_idle(true);
  flash_enable_write_protection();
  flash_prf_set_protection(true);

#ifdef CONFIG_MIC
  mic_init(MIC);
#endif

#ifdef CONFIG_TOUCH
  touch_sensor_init();
#if !defined(RECOVERY_FW)
  // Only keep touch enabled on recovery (and so manufacturing as well)
  // Once supported in main firmware, this should be removed.
  touch_sensor_set_enabled(false);
#endif
#endif

  accel_init();
#ifdef CONFIG_MAG
  mag_init();
#endif
#ifdef CONFIG_PRESSURE
  pressure_init();
#endif

  backlight_init();
  ambient_light_init();

  temperature_init();

  rtc_init_timers();
  rtc_alarm_init();

  power_tracking_init();
}

static void clear_reset_loop_detection_bits(void) {
  boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_ONE);
  boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_TWO);
  boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_THREE);
}

#ifndef MANUFACTURING_FW
static void uptime_callback(void* data) {
  PBL_LOG_VERBOSE("Uptime reached 15 minutes, set stable bit.");
  new_timer_delete(s_uptime_timer);
  boot_bit_set(BOOT_BIT_FW_STABLE);
}
#endif

static void prv_low_power_debug_config_callback(void* data) {
  new_timer_delete(s_lowpower_timer);
}

#ifdef TEST_SJLJ
static jmp_buf s_sjlj_jmpbuf;
static volatile int s_sjlj_num;
static void prv_sjlj_second(int r) {
  PBL_ASSERT(s_sjlj_num == 1, "SJLJ TRACK INCORRECT @ SECOND");
  s_sjlj_num++;
  longjmp(s_sjlj_jmpbuf, 0);
}
static void prv_sjlj_first(int r) {
  PBL_ASSERT(s_sjlj_num == 0, "SJLJ TRACK INCORRECT @ FIRST");
  s_sjlj_num++;
  prv_sjlj_second(r);
  PBL_ASSERT(1, "SJLJ IS BROKEN (longjmp didn't occur)");
}
static void prv_sjlj_main(int r) {
  PBL_ASSERT(s_sjlj_num == 2, "SJLJ TRACK INCORRECT @ MAIN");
  s_sjlj_num++;
  PBL_ASSERT(r == 1, "SETJMP IS BROKEN (longjmp value wasn't correct)");
}
static void prv_test_sjlj(void) {
  int r;
  s_sjlj_num = 0;
  if (!(r = setjmp(s_sjlj_jmpbuf)))
    prv_sjlj_first(r);
  else
    prv_sjlj_main(r);
  PBL_ASSERT(s_sjlj_num == 3, "SJLJ TRACK INCORRECT @ END");
  PBL_LOG_ALWAYS("sjlj works \\o/");
}
#endif

static NOINLINE void prv_main_task_init(void) {
  // The Snowy bootloader does not clear the watchdog flag itself. Clear the
  // flag ourselves so that a future safe reset does not look like a watchdog
  // reset to the bootloader.
  static McuRebootReason s_mcu_reboot_reason;
  s_mcu_reboot_reason = watchdog_clear_reset_flag();

#if PULSE_EVERYWHERE
  pulse_init();
  pulse_logging_init();
#endif

  pebble_task_configure_idle_task();

  task_init();

  memory_layout_setup_mpu();

  board_early_init();

  boot_splash_start();

  kernel_applib_init();

  system_task_init();

  events_init();

  new_timer_service_init();
  regular_timer_init();

  // Initialize the task watchdog and immediately pause it for 30 seconds to
  // give us time to initialize everything without worrying about task watchdog
  // from firing if we block other tasks.
  task_watchdog_init();
  task_watchdog_pause(30);

  analytics_init();
  register_system_timers();
  system_task_timer_init();

  init_drivers();

  clock_init();

#if defined(IS_BIGBOARD)
  // Program a random S/N into the Bigboard in case it's not been done yet:
  mfg_write_bigboard_serial_number();
#endif

#if defined(MANUFACTURING_FW)
  mfg_info_update_constant_data();
#endif

  debug_init(s_mcu_reboot_reason);

  services_early_init();

  debug_print_last_launched_app();

  // Do this early before things can screw ith it.
  check_prf_update();

#if CAPABILITY_HAS_PBLBOOT && defined(RECOVERY_FW) && !defined(MANUFACTURING_FW)
  // Invalidate slot0/1 when booting PRF, so we force main firmware re-install
  firmware_storage_invalidate_firmware_slot(0);
  firmware_storage_invalidate_firmware_slot(1);
#endif

  // When there are new system resources waiting to be installed, this call
  // will actually install them:
  resource_init();

  system_resource_init();

#ifdef CONFIG_HRM
  if (mfg_info_is_hrm_present()) {
    hrm_init(HRM);
  }
#endif

  // The display has to be initialized before bluetooth because on Snowy the
  // display FPGA shares the 32 kHz clock signal with bluetooth. If the FPGA is
  // not programmed when we attempt to initialize bluetooth, it prevents the
  // clock from reaching the bluetooth module and initialization fails.
  display_init();

  // Use the MFG calibrated display offset to adjust the display
  GPoint mfg_offset = mfg_info_get_disp_offsets();
  display_set_offset(mfg_offset);
  // Log display offsets for use in contact support logs
  PBL_LOG_INFO("MFG Display Offsets (%"PRIi16",%"PRIi16").", mfg_offset.x, mfg_offset.y);
  
  // Stop boot splash before initializing compositor
  boot_splash_stop();
  // Can't use the compositor framebuffer until the compositor is initialized
  compositor_init();
  kernel_ui_init();

  bt_driver_init();

  services_init();

  // The RTC needs be calibrated after the mfg registry service has been initialized so we can
  // load the measured frequency.
#if defined(MICRO_FAMILY_SF32LB52) && !defined(SF32LB52_USE_LXT)
  rc10k_init();
#endif
  rtc_calibrate_frequency(mfg_info_get_rtc_freq());

  clear_reset_loop_detection_bits();

  task_watchdog_mask_set(PebbleTask_KernelMain);

  stop_mode_enable(InhibitorMain);

  // Leave the board with stop and sleep mode debugging enabled for at least 10
  // seconds to give OpenOCD time to start and still able to connect when it is
  // ready to flash in the new image via JTAG
  s_lowpower_timer = new_timer_create();
  new_timer_start(s_lowpower_timer,
                  10 * 1000, prv_low_power_debug_config_callback, NULL, 0 /*flags*/);

#ifndef MANUFACTURING_FW
  s_uptime_timer = new_timer_create();
  new_timer_start(s_uptime_timer, 15 * 60 * 1000, uptime_callback, NULL, 0 /*flags*/);
#else
  boot_bit_set(BOOT_BIT_FW_STABLE);
#endif

  // Initialize button driver at the last moment to prevent "system on" button press from
  // entering the kernel event queue.
  debounced_button_init();

#ifdef DUMP_GPIO_CFG_STATE
  // at this point everything should be configured!
  dump_gpio_configuration_state();
#endif

#ifdef TEST_SJLJ
  // Test setjmp/longjmp
  prv_test_sjlj();
#endif

  task_watchdog_resume();
}

static void main_task(void *parameter) {
  prv_main_task_init();
  launcher_main_loop();
}
