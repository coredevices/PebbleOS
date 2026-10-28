/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/pwm.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>

static int prv_parse_pwm(const struct pbl_shell *sh, char **argv, struct pbl_pwm *pwm) {
  const struct pbl_device *dev = pbl_device_find(&pbl_pwm_class, argv[1]);
  const struct pbl_pwm_controller *ctrl;
  unsigned long channel;

  if (dev == nullptr) {
    pbl_shell_error(sh, "unknown controller '%s'", argv[1]);
    return -ENODEV;
  }

  ctrl = container_of(dev, const struct pbl_pwm_controller, dev);
  if (pbl_shell_strtoul(argv[2], &channel) != 0 || channel >= ctrl->num_channels) {
    pbl_shell_error(sh, "invalid channel '%s'", argv[2]);
    return -EINVAL;
  }

  *pwm = (struct pbl_pwm)PBL_PWM(ctrl, (uint8_t)channel, 0U, 0U);

  return 0;
}

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_pwm_class, dev) {
    const struct pbl_pwm_controller *ctrl = container_of(dev, const struct pbl_pwm_controller, dev);

    if (!pbl_device_is_ready(dev)) {
      pbl_shell_print(sh, "%s (not ready)", dev->name);
      continue;
    }

    pbl_shell_print(sh, "%s", dev->name);
    for (uint8_t ch = 0U; ch < ctrl->num_channels; ch++) {
      const struct pbl_pwm_state *state = &ctrl->states[ch];

      pbl_shell_print(sh, "  %u: %s period %lu ns duty %lu ns%s", ch,
                      state->enabled ? "on " : "off", (unsigned long)state->period,
                      (unsigned long)state->duty_cycle,
                      (state->polarity == PBL_PWM_POLARITY_INVERSED) ? " inversed" : "");
    }
  }

  return 0;
}

static int prv_cmd_set(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_pwm_state state;
  unsigned long period;
  unsigned long duty;
  struct pbl_pwm pwm;
  int ret;

  ret = prv_parse_pwm(sh, argv, &pwm);
  if (ret != 0) {
    return ret;
  }

  if (pbl_shell_strtoul(argv[3], &period) != 0 || period == 0UL || period > UINT32_MAX) {
    pbl_shell_error(sh, "invalid period '%s'", argv[3]);
    return -EINVAL;
  }

  if (pbl_shell_strtoul(argv[4], &duty) != 0 || duty > period) {
    pbl_shell_error(sh, "invalid duty cycle '%s'", argv[4]);
    return -EINVAL;
  }

  if (argc > 5U && strcmp(argv[5], "inv") != 0) {
    pbl_shell_error(sh, "unknown flag '%s'", argv[5]);
    return -EINVAL;
  }

  state = (struct pbl_pwm_state){
    .period = (uint32_t)period,
    .duty_cycle = (uint32_t)duty,
    .polarity = (argc > 5U) ? PBL_PWM_POLARITY_INVERSED : PBL_PWM_POLARITY_NORMAL,
    .enabled = true,
  };

  ret = pbl_pwm_apply(&pwm, &state);
  if (ret != 0) {
    pbl_shell_error(sh, "apply failed (%d)", ret);
  }

  return ret;
}

static int prv_cmd_off(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_pwm pwm;
  int ret;

  ret = prv_parse_pwm(sh, argv, &pwm);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pwm_disable(&pwm);
  if (ret != 0) {
    pbl_shell_error(sh, "disable failed (%d)", ret);
  }

  return ret;
}

static const struct pbl_shell_cmd sub_pwm[] = {
  PBL_SHELL_CMD(list, nullptr, "List the controllers and their channels", prv_cmd_list),
  PBL_SHELL_CMD_ARG(set, nullptr, "Run a channel <ctrl> <ch> <period ns> <duty ns> [inv]",
                    prv_cmd_set, 5, 1),
  PBL_SHELL_CMD_ARG(off, nullptr, "Disable a channel <ctrl> <ch>", prv_cmd_off, 3, 0),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(pwm, sub_pwm, "PWM controllers", nullptr);
