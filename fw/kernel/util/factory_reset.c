/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/rtc.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/services/event_service.h>
#include <pbl/services/runlevel.h>
#include <pbl/services/shared_prf_storage/shared_prf_storage.h>
#include <pbl/services/system_task.h>
#include <pbl/task_wdt/task_wdt.h>

#include <flash_region/filesystem_regions.h>
#include <kernel/event_loop.h>
#include <kernel/util/factory_reset.h>
#include <kernel/util/standby.h>
#include <process_management/worker_manager.h>
#include <shell/normal/app_idle_timeout.h>
#include <system/bootbits.h>
#include <system/firmware_storage.h>
#include <system/reboot_reason.h>
#include <system/reset.h>

#if !defined(CONFIG_RECOVERY_FW)
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/services/blob_db/pin_db.h>
#include <pbl/services/blob_db/reminder_db.h>
#include <pbl/services/timeline/event.h>

#include <kernel/pbl_malloc.h>
#endif

static bool s_in_factory_reset = false;

static void prv_factory_reset_non_pfs_data() {
  PBL_LOG_SYNC_INFO("Factory resetting...");

  // This function can block the system task for a long time.
  // Prevent callbacks being added to the system task so it doesn't overflow.
  system_task_block_callbacks(true /* block callbacks */);
  launcher_block_popups(true);

  worker_manager_disable();
  event_service_clear_process_subscriptions(PebbleTask_App);

  shared_prf_storage_wipe_all();

  services_set_runlevel(RunLevel_BareMinimum);
  app_idle_timeout_stop();

  while (worker_manager_get_current_worker_md()) {
    // busy loop until the worker is killed
    pbl_sleep_ms(3);
  }

  rtc_timezone_clear();
}

void factory_reset_set_reason_and_reset(void) {
  RebootReason reason = {RebootReasonCode_FactoryResetReset, 0};
  reboot_reason_set(&reason);
  system_reset();
}

static void prv_factory_reset_post(bool should_shutdown) {
  // Reset instead if the watch can't power off, e.g. while charging
  if (should_shutdown) {
    enter_standby(RebootReasonCode_FactoryResetShutdown);
  }
  factory_reset_set_reason_and_reset();
}

void factory_reset(bool should_shutdown) {
  s_in_factory_reset = true;

  prv_factory_reset_non_pfs_data();

#if !defined(CONFIG_RECOVERY_FW)
  // Commit to booting PRF before the filesystem wipe: it can take minutes,
  // and a reset in that window (watchdog, crash, power loss) must land in
  // PRF instead of rebooting a half-wiped normal firmware.
  // "First use" is part of the PRF image for Snowy.
  boot_bit_set(BOOT_BIT_FORCE_PRF);
#ifdef CONFIG_PBLBOOT
  // Invalidate both firmware slots so the bootloader doesn't boot into them
  firmware_storage_invalidate_firmware_slot(0);
  firmware_storage_invalidate_firmware_slot(1);
#endif

  // Formatting PFS holds its mutex across the erase, blocking concurrent
  // writes from the App task that would otherwise survive into a freshly-erased region.
  pbl_fs_format("/");
#else
  filesystem_regions_erase_all();
#endif

  prv_factory_reset_post(should_shutdown);
}

#if !defined(CONFIG_RECOVERY_FW)
void close_db_files() {
  // Deinit the databases and any clients
  timeline_event_deinit();
  reminder_db_deinit();
  pin_db_deinit();
}

static void prv_remove_all_files(void) {
  struct pbl_fs_dir dir;
  if (pbl_fs_opendir(&dir, "/") < 0) {
    return;
  }

  struct pbl_fs_dirent *entry = kernel_malloc_check(sizeof(*entry));
  char *path = kernel_malloc_check(sizeof("/") + PBL_FS_NAME_MAX);
  while ((pbl_fs_readdir(&dir, entry) == 0) && (entry->name[0] != '\0')) {
    strcpy(path, "/");
    strcat(path, entry->name);
    pbl_fs_unlink(path);
  }

  kernel_free(path);
  kernel_free(entry);
  pbl_fs_closedir(&dir);
}

void factory_reset_fast(void *unused) {
  s_in_factory_reset = true;

  // Lots of flash work follows before the reset.
  pbl_task_wdt_suspend(0);

  close_db_files();

  prv_factory_reset_non_pfs_data();

  prv_remove_all_files();

  prv_factory_reset_post(false /* should_shutdown */);
}
#endif // !defined(CONFIG_RECOVERY_FW)

bool factory_reset_ongoing(void) {
  return s_in_factory_reset;
}
