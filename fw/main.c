/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/bluetooth/init.h>
#include <pbl/board/partitions.h>
#include <pbl/device.h>
#include <pbl/drivers/battery.h>
#include <pbl/drivers/flash.h>
#include <pbl/drivers/hrm.h>
#include <pbl/drivers/vibe.h>
#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/mpu.h>
#include <pbl/kernel/thread.h>
#include <pbl/logging/backend.h>
#include <pbl/logging/backend_flash.h>
#include <pbl/logging/logging.h>
#include <pbl/services/analytics/analytics.h>
#include <pbl/services/boot_splash.h>
#include <pbl/services/clock.h>
#include <pbl/services/compositor/compositor.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/services/new_timer/new_timer_service.h>
#include <pbl/services/prf_update.h>
#include <pbl/services/regular_timer.h>
#include <pbl/services/services.h>
#include <pbl/services/system_task.h>
#include <pbl/task_wdt/task_wdt.h>

#include <debug/debug.h>
#include <kernel/event_loop.h>
#include <kernel/events.h>
#include <kernel/input_buttons.h>
#include <kernel/kernel_applib_state.h>
#include <kernel/pebble_tasks.h>
#include <kernel/ui/kernel_ui.h>
#include <kernel/util/task_init.h>
#include <mfg/mfg_info.h>
#include <process_management/process_regions.h>
#include <resource/resource.h>
#include <resource/system_resource.h>
#include <system/bootbits.h>
#include <system/firmware_storage.h>
#include <system/version.h>
#ifdef CONFIG_QEMU_SERIAL
#include <comm/qemu/serial.h>
#endif

#ifndef CONFIG_MFG
static TimerID s_uptime_timer = TIMER_INVALID_ID;
#endif

static void print_splash_screen(void) {
#if defined(CONFIG_MFG)
  PBL_LOG_ALWAYS("Manufacturing mode");
#elifdef CONFIG_RECOVERY_FW
  PBL_LOG_ALWAYS("Recovery mode");
#endif
  if (TINTIN_METADATA.is_dual_slot && !TINTIN_METADATA.is_recovery_firmware) {
    PBL_LOG_ALWAYS("Slot %d", TINTIN_METADATA.is_slot_0 ? 0 : 1);
  }
  PBL_LOG_ALWAYS("(c) 2013-2026 The PebbleOS contributors");
  PBL_LOG_ALWAYS(" ");
}

static void init_drivers(void) {
  pbl_device_init_all();

  battery_init();

  (void)pbl_flash_partition_protect(PART_SAFE_FIRMWARE);

  uint8_t vibe_cali = mfg_info_get_vibe_cali();
  if (vibe_cali != MFG_INFO_VIBE_CALI_INVALID) {
    (void)pbl_vibe_set_calibration(VIBE, vibe_cali);
  }
}

static void clear_reset_loop_detection_bits(void) {
  boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_ONE);
  boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_TWO);
  boot_bit_clear(BOOT_BIT_RESET_LOOP_DETECT_THREE);
}

#ifndef CONFIG_MFG
static void uptime_callback(void *data) {
  PBL_LOG_VERBOSE("Uptime reached 15 minutes, set stable bit.");
  new_timer_delete(s_uptime_timer);
  boot_bit_set(BOOT_BIT_FW_STABLE);
}
#endif

#ifdef CONFIG_SOC_SF32LB52
// The linker script places the image from Kconfig, which can't see the partitions
#if defined(CONFIG_RECOVERY_FW) && !defined(CONFIG_MFG) && !defined(CONFIG_RECOVERY_FW_AS_FW)
static_assert(CONFIG_FLASH_OFFSET == PART_SAFE_FIRMWARE_OFFSET);
static_assert(CONFIG_FW_FLASH_SIZE == PART_SAFE_FIRMWARE_SIZE);
#elif CONFIG_FIRMWARE_SLOT == 1
static_assert(CONFIG_FLASH_OFFSET == PART_FIRMWARE_SLOT_1_OFFSET);
static_assert(CONFIG_FW_FLASH_SIZE == PART_FIRMWARE_SLOT_1_SIZE);
#else
static_assert(CONFIG_FLASH_OFFSET == PART_FIRMWARE_SLOT_0_OFFSET);
static_assert(CONFIG_FW_FLASH_SIZE == PART_FIRMWARE_SLOT_0_SIZE);
#endif
#endif

static void prv_resource_banks_region_add(void) {
#ifdef CONFIG_MMAP_RESOURCES
  static_assert(PART_SYSTEM_RESOURCES_BANK_0_OFFSET + PART_SYSTEM_RESOURCES_BANK_0_SIZE ==
                    PART_SYSTEM_RESOURCES_BANK_1_OFFSET,
                "resource banks must be contiguous");

  // System resources are served as pointers into the memory mapped banks.
  static const struct pbl_mpu_region s_region = {
    .name = "resource banks",
    .base = CONFIG_FLASH_BASE + PART_SYSTEM_RESOURCES_BANK_0_OFFSET,
    .size = PART_SYSTEM_RESOURCES_BANK_0_SIZE + PART_SYSTEM_RESOURCES_BANK_1_SIZE,
    .attr = PBL_MPU_P_RO_U_RO | PBL_MPU_NORMAL_WT | PBL_MPU_SHAREABLE,
  };
  PBL_ASSERTN(pbl_mpu_region_add(&s_region) == 0);
#endif
}

int main(void) {
  pebble_task_register(PebbleTask_KernelMain, pbl_thread_current());
  process_regions_thread_init();

  print_splash_screen();

  clock_restore_time();

#ifdef CONFIG_RECOVERY_FW
  boot_bit_clear(BOOT_BIT_RECOVERY_START_IN_PROGRESS);
#endif

  static McuRebootReason s_mcu_reboot_reason;
  s_mcu_reboot_reason = watchdog_clear_reset_flag();

  pbl_log_init();

  task_init();

  prv_resource_banks_region_add();

  kernel_applib_init();

  system_task_init();

  events_init();

  new_timer_service_init();
  regular_timer_init();

  // After the timer service, which the display may use
  boot_splash_start();

  // Suspend the task watchdog while the rest of the system comes up.
  pbl_task_wdt_init();
  pbl_task_wdt_suspend(30 * 1000);

  pbl_analytics_init();
  system_task_timer_init();

  init_drivers();

  clock_init();

#if defined(CONFIG_MFG)
  mfg_info_update_constant_data();
#endif

#ifdef CONFIG_LOG_BACKEND_FLASH
  pbl_log_flash_init();
#endif

  debug_init(s_mcu_reboot_reason);

  services_early_init();

  debug_print_last_launched_app();

  // Do this early before things can screw ith it.
  check_prf_update();

#if defined(CONFIG_PBLBOOT) && defined(CONFIG_RECOVERY_FW) && !defined(CONFIG_MFG)
  // Invalidate slot0/1 when booting PRF, so we force main firmware re-install
  firmware_storage_invalidate_firmware_slot(0);
  firmware_storage_invalidate_firmware_slot(1);
#endif

  // When there are new system resources waiting to be installed, this call
  // will actually install them:
  resource_init();

  system_resource_init();

#ifdef CONFIG_HRM
  hrm_init(HRM);
#endif

  // Stop boot splash before initializing compositor
  boot_splash_stop();
  // Can't use the compositor framebuffer until the compositor is initialized
  compositor_init();
  kernel_ui_init();

#ifdef CONFIG_QEMU_SERIAL
  qemu_serial_init();
#endif
  pbl_bt_init();

  services_init();

  clear_reset_loop_detection_bits();

  PBL_ASSERTN(pbl_task_wdt_add(nullptr, CONFIG_TASK_WDT_TIMEOUT_MS, nullptr, nullptr) >= 0);

#ifndef CONFIG_MFG
  s_uptime_timer = new_timer_create();
  new_timer_start(s_uptime_timer, 15 * 60 * 1000, uptime_callback, nullptr, 0 /*flags*/);
#else
  boot_bit_set(BOOT_BIT_FW_STABLE);
#endif

  input_buttons_enable();

  pbl_task_wdt_resume();

  launcher_main_loop();
  return 0;
}
