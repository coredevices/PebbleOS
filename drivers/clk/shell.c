/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <math.h>

#include <pbl/drivers/clk.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>

#ifdef CONFIG_CLK_CAL
#include <pbl/drivers/clk/cal.h>
#endif

static const struct pbl_clk_controller *prv_ctrl(const struct pbl_shell *sh, const char *name) {
  const struct pbl_device *dev = pbl_device_find(&pbl_clk_controller_class, name);

  if (dev == nullptr) {
    pbl_shell_error(sh, "no clock controller '%s'", name);
    return nullptr;
  }

  return container_of(dev, const struct pbl_clk_controller, dev);
}

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_clk_controller_class, dev) {
    const struct pbl_clk_controller *ctrl = container_of(dev, const struct pbl_clk_controller, dev);

    pbl_shell_print(sh, "%s: %u clocks", dev->name, ctrl->num_clks);
    for (uint16_t id = 0U; id < ctrl->num_clks; id++) {
      const struct pbl_clk clk = PBL_CLK(ctrl, id);
      uint64_t rate_mhz;

      if (pbl_clk_get_rate(&clk, &rate_mhz) == 0) {
        pbl_shell_print(sh, "  %u: %" PRIu64 " mHz", id, rate_mhz);
      }
    }
  }

  return 0;
}

static int prv_cmd_rate(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_clk_controller *ctrl = prv_ctrl(sh, argv[1]);
  uint64_t rate_mhz;
  long id;
  int ret;

  if (ctrl == nullptr) {
    return -ENODEV;
  }
  if (pbl_shell_strtol(argv[2], &id) != 0 || id < 0) {
    pbl_shell_error(sh, "invalid clock '%s'", argv[2]);
    return -EINVAL;
  }

  const struct pbl_clk clk = PBL_CLK(ctrl, (uint32_t)id);

  ret = pbl_clk_get_rate(&clk, &rate_mhz);
  if (ret != 0) {
    pbl_shell_error(sh, "no rate (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "%" PRIu64 " mHz", rate_mhz);

  return 0;
}

#ifdef CONFIG_CLK_CAL
static struct pbl_clk_cal *prv_cal(const struct pbl_shell *sh, const char *name) {
  struct pbl_clk_cal *cal = pbl_clk_cal_find(name);

  if (cal == nullptr) {
    pbl_shell_error(sh, "no calibrated clock '%s'", name);
  }

  return cal;
}

static int prv_cmd_cal_status(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_clk_cal *cal = prv_cal(sh, argv[1]);
  struct pbl_clk_cal_status st;

  if (cal == nullptr) {
    return -ENODEV;
  }

  pbl_clk_cal_get_status(cal, &st);
  pbl_shell_print(sh, "uptime %" PRIu64 " ms", pbl_uptime_ms());
  pbl_shell_print(sh, "rate %" PRIu64 " mHz%s, tracked %" PRIu64 " mHz, sigma %" PRIu32 " ppb",
                  st.rate_mhz, st.held ? " (held)" : "", st.tracked_mhz, st.sigma_ppb);
  pbl_shell_print(sh, "gates %" PRIu32 " used, %" PRIu32 " rejected, %" PRIu32 " failed",
                  st.accepted, st.rejected, st.failed);

  return 0;
}

// A header, then one line per gate: uptime (ms) and frequency (mHz), for
// tools/clk_characterize.py.
static int prv_cmd_cal_sample(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_clk_cal *cal = prv_cal(sh, argv[1]);
  long cycles;
  long count;
  long period_ms = 0;

  if (cal == nullptr) {
    return -ENODEV;
  }
  if (pbl_shell_strtol(argv[2], &cycles) != 0 || cycles <= 0 ||
      pbl_shell_strtol(argv[3], &count) != 0 || count <= 0 ||
      (argc > 4 && (pbl_shell_strtol(argv[4], &period_ms) != 0 || period_ms < 0))) {
    pbl_shell_error(sh, "usage: <cycles> <count> [period_ms]");
    return -EINVAL;
  }

  pbl_shell_print(sh, "gate %ld cycles", cycles);
  for (long i = 0; i < count; i++) {
    uint64_t t_ms = pbl_uptime_ms();
    double hz;
    int ret;

    ret = pbl_clk_cal_sample(cal, (uint32_t)cycles, &hz);
    if (ret != 0) {
      pbl_shell_print(sh, "%" PRIu64 " error %d", t_ms, ret);
    } else {
      pbl_shell_print(sh, "%" PRIu64 " %" PRIu64, t_ms, (uint64_t)llround(hz * 1000.0));
    }
    if (period_ms > 0) {
      pbl_thread_sleep(PBL_MSEC((uint32_t)period_ms));
    }
  }

  return 0;
}

static int prv_cmd_cal_hold(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_clk_cal *cal = prv_cal(sh, argv[1]);
  long rate_mhz;

  if (cal == nullptr) {
    return -ENODEV;
  }
  if (pbl_shell_strtol(argv[2], &rate_mhz) != 0 || rate_mhz < 0) {
    pbl_shell_error(sh, "invalid rate '%s'", argv[2]);
    return -EINVAL;
  }

  pbl_clk_cal_hold(cal, (uint64_t)rate_mhz);

  return 0;
}

static const struct pbl_shell_cmd sub_clk_cal[] = {
  PBL_SHELL_CMD_ARG(status, nullptr, "Show the calibration state <name>", prv_cmd_cal_status, 2, 0),
  PBL_SHELL_CMD_ARG(sample, nullptr, "Take raw gates <name> <cycles> <count> [period_ms]",
                    prv_cmd_cal_sample, 4, 1),
  PBL_SHELL_CMD_ARG(hold, nullptr, "Hold the rate <name> <mHz>, 0 to release", prv_cmd_cal_hold, 3,
                    0),
  PBL_SHELL_SUBCMD_SET_END,
};
#endif

static const struct pbl_shell_cmd sub_clk[] = {
  PBL_SHELL_CMD(list, nullptr, "List the clock controllers and the known rates", prv_cmd_list),
  PBL_SHELL_CMD_ARG(rate, nullptr, "Show a clock rate <controller> <id>", prv_cmd_rate, 3, 0),
#ifdef CONFIG_CLK_CAL
  PBL_SHELL_CMD(cal, sub_clk_cal, "Calibrated clocks", nullptr),
#endif
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(clk, sub_clk, "Clocks", nullptr);
