/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "kernel/util/fw_reset.h"

#include "console/pulse_internal.h"
#include "kernel/core_dump.h"
#include "kernel/util/factory_reset.h"
#include "services/common/comm_session/session.h"
#include "services/common/system_task.h"
#include "services/runlevel.h"
#include "system/bootbits.h"
#include "system/logging.h"
#include "system/passert.h"
#include "system/reset.h"

static void prv_reset_into_prf(void) {
  RebootReason reason = { RebootReasonCode_PrfReset, 0 };
  reboot_reason_set(&reason);
  boot_bit_set(BOOT_BIT_FORCE_PRF);
  services_set_runlevel(RunLevel_BareMinimum);
  system_reset();
}

void fw_reset_into_prf(void) {
  prv_reset_into_prf();
}

static const uint8_t s_prf_reset_cmd __attribute__((unused)) = 0xff;

typedef enum {
  ResetCmdNormal = 0x00,
  ResetCmdCoreDump = 0x01,
  ResetCmdFactoryReset = 0xfe,
  ResetCmdIntoRecovery = 0xff,
} ResetCmd;

void reset_protocol_msg_callback(CommSession *session, const uint8_t* data, unsigned int length) {
  PBL_ASSERT_RUNNING_FROM_EXPECTED_TASK(PebbleTask_KernelBackground);

  const uint8_t cmd = data[0];

  switch (cmd) {
    case ResetCmdNormal:
      PBL_LOG(LOG_LEVEL_WARNING, "Rebooting");
      system_reset();
      break;

    case ResetCmdCoreDump:
      PBL_LOG(LOG_LEVEL_INFO, "Core dump + Reboot triggered");
      core_dump_reset(true /* force overwrite any existing core dump */);
      break;

    case ResetCmdIntoRecovery:
      PBL_LOG(LOG_LEVEL_WARNING, "Rebooting into PRF");
      prv_reset_into_prf();
      break;

    case ResetCmdFactoryReset:
      factory_reset(false /* should_shutdown */);
      break;

    default:
      PBL_LOG(LOG_LEVEL_ERROR, "Invalid reset msg, data[0] %u", data[0]);
      break;
  }
}

void fw_prepare_for_reset(bool unsafe_reset) {
  if (!unsafe_reset) {
    // Tear down Bluetooth, to avoid confusing the phone:
    services_set_runlevel(RunLevel_BareMinimum);
#if PULSE_EVERYWHERE
    pulse_end();
#endif
  } else {
    pulse_prepare_to_crash();
  }
}

