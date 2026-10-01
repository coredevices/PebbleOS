/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_command.h"

#if defined(CONFIG_SHELL) && defined(CONFIG_RECOVERY_FW)
#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include <pbl/shell/shell.h>

#include "kernel/util/factory_reset.h"
#include "kernel/util/standby.h"
#include "mfg/mfg_info.h"
#include "system/bootbits.h"

#ifdef CONFIG_MFG
#define MFG_WRITE_OPT 1
#else
#define MFG_WRITE_OPT 0
#endif

static int prv_cmd_standby(const struct pbl_shell *sh, size_t argc, char **argv) {
  enter_standby(RebootReasonCode_MfgShutdown);
  return 0;
}

static int prv_cmd_consumer(const struct pbl_shell *sh, size_t argc, char **argv) {
  boot_bit_set(BOOT_BIT_FORCE_PRF);
  factory_reset(true /* should_shutdown */);
  return 0;
}

static int prv_cmd_color(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (argc == 1) {
    pbl_shell_print(sh, "%d", mfg_info_get_watch_color());
    return 0;
  }

  long color;
  if (pbl_shell_strtol(argv[1], &color) != 0) {
    pbl_shell_error(sh, "invalid color '%s'", argv[1]);
    return -EINVAL;
  }

  mfg_info_set_watch_color(color);
  if (mfg_info_get_watch_color() != color) {
    pbl_shell_error(sh, "write failed");
    return -EIO;
  }

  pbl_shell_print(sh, "OK");
  return 0;
}

static int prv_cmd_rtcfreq(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (argc == 1) {
    pbl_shell_print(sh, "%" PRIu32, mfg_info_get_rtc_freq());
    return 0;
  }

  unsigned long rtc_freq;
  if (pbl_shell_strtoul(argv[1], &rtc_freq) != 0) {
    pbl_shell_error(sh, "invalid rtcfreq '%s'", argv[1]);
    return -EINVAL;
  }

  mfg_info_set_rtc_freq(rtc_freq);
  return 0;
}

static int prv_cmd_model(const struct pbl_shell *sh, size_t argc, char **argv) {
  char model[MFG_INFO_MODEL_STRING_LENGTH];

  if (argc == 1) {
    mfg_info_get_model(model);
    pbl_shell_print(sh, "%s", model);
    return 0;
  }

  mfg_info_set_model(argv[1]);
  mfg_info_get_model(model);
  if (strncmp(argv[1], model, MFG_INFO_MODEL_STRING_LENGTH) != 0) {
    pbl_shell_error(sh, "write failed");
    return -EIO;
  }

  pbl_shell_print(sh, "OK");
  return 0;
}

PBL_SHELL_SUBCMD_SET_CREATE(sub_mfg);
PBL_SHELL_CMD_REGISTER(mfg, sub_mfg, "Manufacturing", NULL);
PBL_SHELL_SUBCMD_ADD(sub_mfg, standby, NULL, "Enter standby", prv_cmd_standby, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_mfg, consumer, NULL, "Factory reset into the consumer PRF",
                     prv_cmd_consumer, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_mfg, color, NULL, "Read or write the watch color [color]", prv_cmd_color,
                     1, MFG_WRITE_OPT);
PBL_SHELL_SUBCMD_ADD(sub_mfg, rtcfreq, NULL, "Read or write the RTC frequency [freq]",
                     prv_cmd_rtcfreq, 1, MFG_WRITE_OPT);
PBL_SHELL_SUBCMD_ADD(sub_mfg, model, NULL, "Read or write the model [model]", prv_cmd_model, 1,
                     MFG_WRITE_OPT);
#endif
