/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/analytics/analytics.h>
#include <pbl/bluetooth/init.h>
#include <pbl/board/partitions.h>
#include <pbl/drivers/flash.h>
#include <pbl/drivers/hrm.h>
#include <pbl/drivers/vibe.h>
#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/mpu.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/backend.h>
#include <pbl/logging/backend_flash.h>
#include <pbl/logging/logging.h>
#include <pbl/services/boot_splash.h>
#include <pbl/services/clock.h>
#include <pbl/services/compositor/compositor.h>
#include <pbl/services/prf_update.h>
#include <pbl/services/services.h>
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
#include <nimble/nimble_port_pebble.h>
#include <process_management/app_throttle.h>
#include <process_management/process_regions.h>
#include <resource/resource.h>
#include <resource/system_resource.h>
#include <system/bootbits.h>
#include <system/firmware_storage.h>
#include <system/version.h>
#ifdef CONFIG_QEMU_SERIAL
#include <comm/qemu/serial.h>
#endif

#if defined(CONFIG_MFG)
#define MODE ", manufacturing mode"
#elifdef CONFIG_RECOVERY_FW
#define MODE ", recovery mode"
#else
#define MODE ""
#endif

static void print_splash_screen(void) {
  const char *slot = "";

  if (TINTIN_METADATA.is_dual_slot && !TINTIN_METADATA.is_recovery_firmware) {
    slot = TINTIN_METADATA.is_slot_0 ? ", slot 0" : ", slot 1";
  }

  PBL_LOG_ALWAYS("PebbleOS %s" MODE "%s", TINTIN_METADATA.version_tag, slot);
}

static void init_drivers(void) {
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
static void uptime_callback(struct pbl_work *work) {
  PBL_LOG_VERBOSE("Uptime reached 15 minutes, set stable bit.");
  boot_bit_set(BOOT_BIT_FW_STABLE);
}

static PBL_DELAYED_WORK_DEFINE(s_uptime_timer, uptime_callback);
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

static void prv_task_init(void *data) {
  task_init();
}

//! The kernel runs the system work queues; give them the system's watchdog and identities.
static void prv_sys_workq_init(void) {
  pebble_task_register(PebbleTask_SysWorkq, pbl_workq_get_thread(&pbl_sys_workq));
  pebble_task_register(PebbleTask_SysWorkqHigh, pbl_workq_get_thread(&pbl_sys_workq_high));
  app_throttle_init();
  PBL_ASSERTN(pbl_task_wdt_add_workq(&pbl_sys_workq_high, CONFIG_TASK_WDT_TIMEOUT_MS, nullptr,
                                     nullptr) >= 0);
  // rand() keeps its state per thread.
  pbl_sys_work_call(prv_task_init, nullptr);
  PBL_ASSERTN(pbl_sys_work_call_high(prv_task_init, nullptr));
}

int main(void) {
  pebble_task_register(PebbleTask_KernelMain, pbl_thread_current());
  process_regions_thread_init();

  print_splash_screen();

  clock_restore_time();

#ifdef CONFIG_RECOVERY_FW
  boot_bit_clear(BOOT_BIT_RECOVERY_START_IN_PROGRESS);
#endif

  static struct pbl_mcu_reboot_reason s_mcu_reboot_reason;
  s_mcu_reboot_reason = watchdog_clear_reset_flag();

  task_init();

  prv_resource_banks_region_add();

  kernel_applib_init();

  prv_sys_workq_init();

  events_init();

  // After the timer service, which the display may use
  boot_splash_start();

  // Suspend the task watchdog while the rest of the system comes up.
  pbl_task_wdt_suspend(30 * 1000);

#ifdef CONFIG_ANALYTICS
  pbl_analytics_init();
#endif

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
  pebble_task_register(PebbleTask_BTHost, nimble_port_pebble_host_thread());
#if NIMBLE_CFG_CONTROLLER
  pebble_task_register(PebbleTask_BTController, nimble_port_pebble_ll_thread());
#endif
#if defined(CONFIG_NIMBLE_HCI_SF32LB) || defined(CONFIG_NIMBLE_HCI_UART)
  pebble_task_register(PebbleTask_BTHCI, nimble_port_pebble_hci_thread());
#endif

  services_init();

  clear_reset_loop_detection_bits();

  PBL_ASSERTN(pbl_task_wdt_add(nullptr, CONFIG_TASK_WDT_TIMEOUT_MS, nullptr, nullptr) >= 0);

#ifndef CONFIG_MFG
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_uptime_timer, PBL_MSEC(15 * 60 * 1000));
#else
  boot_bit_set(BOOT_BIT_FW_STABLE);
#endif

  input_buttons_enable();

  pbl_task_wdt_resume();

  launcher_main_loop();
  return 0;
}
