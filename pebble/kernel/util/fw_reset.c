/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/reboot.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/endpoint.h>
#include <pbl/pp/session.h>
#include <pbl/services/runlevel.h>

#include <apps/core/progress_ui.h>
#include <kernel/event_loop.h>
#include <kernel/pebble_tasks.h>
#include <kernel/ui/modals/modal_manager.h>
#include <kernel/util/factory_reset.h>
#include <kernel/util/fw_reset.h>
#include <process_management/app_manager.h>
#include <system/bootbits.h>
#include <system/die.h>
#include <system/reboot_reason.h>

static void prv_reset_into_prf(void) {
  RebootReason reason = {RebootReasonCode_PrfReset, 0};
  reboot_reason_set(&reason);
  boot_bit_set(BOOT_BIT_FORCE_PRF);
  services_set_runlevel(RunLevel_BareMinimum);
  pbl_reboot();
}

void fw_reset_into_prf(void) {
  prv_reset_into_prf();
}

[[maybe_unused]] static const uint8_t s_prf_reset_cmd = 0xff;

typedef enum {
  ResetCmdNormal = 0x00,
  ResetCmdCoreDump = 0x01,
  ResetCmdFactoryReset = 0xfe,
  ResetCmdIntoRecovery = 0xff,
} ResetCmd;

static void prv_launch_factory_reset_app(void *unused) {
  // factory_reset() holds the PFS mutex for the entire erase (~3 min via
  // pbl_fs_format()). launcher_block_popups() in prv_factory_reset_non_pfs_data
  // only blocks new popups -- it does not dismiss ones already on screen.
  // Pop them here so the user can't, say, press Select on an open notification
  // and block KernelMain in pin_db_get -> pbl_fs_seek on the held PFS mutex.
  modal_manager_pop_all();

  static const ProgressUIAppArgs s_factory_reset_args = {
    .progress_source = PROGRESS_UI_SOURCE_FACTORY_RESET,
  };
  app_manager_launch_new_app(&(AppLaunchConfig){
    .md = progress_ui_app_get_info(),
    .common.args = &s_factory_reset_args,
    .restart = true,
  });
}

void reset_protocol_msg_callback(struct pbl_pp_session *session, const uint8_t *data,
                                 size_t length) {
  PBL_ASSERT_RUNNING_FROM_EXPECTED_TASK(PebbleTask_SysWorkq);

  const uint8_t cmd = data[0];

  switch (cmd) {
    case ResetCmdNormal:
      PBL_LOG_WRN("Rebooting");
      pbl_reboot();
      break;

    case ResetCmdCoreDump:
      PBL_LOG_WRN("Core dump + Reboot triggered");
      reset_with_core_dump(true /* force overwrite any existing core dump */);
      break;

    case ResetCmdIntoRecovery:
      PBL_LOG_WRN("Rebooting into PRF");
      prv_reset_into_prf();
      break;

    case ResetCmdFactoryReset:
      launcher_task_add_callback(prv_launch_factory_reset_app, nullptr);
      factory_reset(false /* should_shutdown */);
      break;

    default:
      PBL_LOG_ERR("Invalid reset msg, data[0] %u", data[0]);
      break;
  }
}

PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_RESET, PBL_PP_ACCESS_PRIVATE, reset_protocol_msg_callback);
