/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/uart.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

static const char *const s_parities[] = {
  [PBL_UART_PARITY_NONE] = "none",
  [PBL_UART_PARITY_ODD] = "odd",
  [PBL_UART_PARITY_EVEN] = "even",
};

static const char *const s_flow_ctrls[] = {
  [PBL_UART_FLOW_CTRL_NONE] = "none",
  [PBL_UART_FLOW_CTRL_RTS_CTS] = "rts-cts",
};

static int prv_parse_name(const struct pbl_shell *sh, const char *arg, const char *const *names,
                          size_t num_names, const char *what) {
  for (size_t i = 0U; i < num_names; i++) {
    if (strcmp(arg, names[i]) == 0) {
      return (int)i;
    }
  }

  pbl_shell_error(sh, "invalid %s '%s'", what, arg);

  return -EINVAL;
}

static const struct pbl_uart *prv_find(const struct pbl_shell *sh, const char *name) {
  const struct pbl_device *dev = pbl_device_find(&pbl_uart_class, name);

  if (dev == nullptr) {
    pbl_shell_error(sh, "unknown port '%s'", name);
    return nullptr;
  }

  return container_of(dev, const struct pbl_uart, dev);
}

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_uart_class, dev) {
    const struct pbl_uart *uart = container_of(dev, const struct pbl_uart, dev);
    struct pbl_uart_config cfg;

    if (pbl_uart_config_get(uart, &cfg) != 0) {
      pbl_shell_print(sh, "%s: not ready", dev->name);
      continue;
    }

    pbl_shell_print(sh, "%s: %lu baud, parity %s, flow control %s, rx %s", dev->name,
                    (unsigned long)cfg.baudrate, s_parities[cfg.parity],
                    s_flow_ctrls[cfg.flow_ctrl], uart->state->rx_enabled ? "on" : "off");
  }

  return 0;
}

static int prv_cmd_config(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_uart *uart = prv_find(sh, argv[1]);
  struct pbl_uart_config cfg = {0};
  unsigned long baudrate;
  int ret;

  if (uart == nullptr) {
    return -ENODEV;
  }

  if (pbl_shell_strtoul(argv[2], &baudrate) != 0 || baudrate == 0UL || baudrate > UINT32_MAX) {
    pbl_shell_error(sh, "invalid baud rate '%s'", argv[2]);
    return -EINVAL;
  }
  cfg.baudrate = (uint32_t)baudrate;

  if (argc > 3) {
    ret = prv_parse_name(sh, argv[3], s_parities, ARRAY_LENGTH(s_parities), "parity");
    if (ret < 0) {
      return ret;
    }
    cfg.parity = (enum pbl_uart_parity)ret;
  }

  if (argc > 4) {
    ret = prv_parse_name(sh, argv[4], s_flow_ctrls, ARRAY_LENGTH(s_flow_ctrls), "flow control");
    if (ret < 0) {
      return ret;
    }
    cfg.flow_ctrl = (enum pbl_uart_flow_ctrl)ret;
  }

  ret = pbl_uart_configure(uart, &cfg);
  if (ret != 0) {
    pbl_shell_error(sh, "configure failed (%d)", ret);
    return ret;
  }

  return 0;
}

static int prv_cmd_write(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_uart *uart = prv_find(sh, argv[1]);

  if (uart == nullptr) {
    return -ENODEV;
  }

  for (size_t i = 2U; i < argc; i++) {
    for (const char *c = argv[i]; *c != '\0'; c++) {
      pbl_uart_poll_out(uart, (uint8_t)*c);
    }
    pbl_uart_poll_out(uart, (i + 1U < argc) ? ' ' : '\n');
  }
  pbl_uart_flush(uart);

  return 0;
}

static const struct pbl_shell_cmd sub_uart[] = {
  PBL_SHELL_CMD(list, nullptr, "List the ports", prv_cmd_list),
  PBL_SHELL_CMD_ARG(config, nullptr,
                    "Set line settings <port> <baud> [none|odd|even] [none|rts-cts]",
                    prv_cmd_config, 3, 2),
  PBL_SHELL_CMD_ARG(write, nullptr, "Send a line of text <port> <word>...", prv_cmd_write, 3,
                    PBL_SHELL_OPT_ARG_MAX),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(uart, sub_uart, "Serial ports", nullptr);
