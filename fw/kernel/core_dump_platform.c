/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/coredump/coredump.h>
#include <pbl/util/build_id.h>

#include <mfg/mfg_serials.h>
#include <system/bootbits.h>
#include <system/reboot_reason.h>
#include <system/version.h>

extern const ElfExternalNote TINTIN_BUILD_ID;

void pbl_coredump_platform_prepare(struct pbl_coredump_metadata *metadata) {
  // Without pblboot the dump overwrites a firmware update that is waiting for a reboot. Forget
  // about it, or loading its resources would drop us to PRF.
  if (boot_bit_test(BOOT_BIT_NEW_FW_AVAILABLE)) {
    boot_bit_clear(BOOT_BIT_NEW_FW_AVAILABLE);
    boot_bit_clear(BOOT_BIT_NEW_SYSTEM_RESOURCES_AVAILABLE);
  }

  RebootReason reason;
  reboot_reason_get(&reason);
  if (reason.code == RebootReasonCode_Unknown) {
    reason = (RebootReason){RebootReasonCode_CoreDump, 0};
    reboot_reason_set(&reason);
  }

  strncpy(metadata->serial_number, mfg_get_serial_number(), sizeof(metadata->serial_number));
  version_copy_build_id_hex_string(metadata->build_id, sizeof(metadata->build_id),
                                   &TINTIN_BUILD_ID);
}
