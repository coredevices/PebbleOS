/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include <pbl/drivers/gpio.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

static int prv_parse_pin(const struct pbl_shell *sh, char **argv, struct pbl_gpio *gpio) {
  const struct pbl_device *dev = pbl_device_find(&pbl_gpio_port_class, argv[1]);
  const struct pbl_gpio_port *port;
  unsigned long pin;

  if (dev == nullptr) {
    pbl_shell_error(sh, "unknown port '%s'", argv[1]);
    return -ENODEV;
  }

  port = container_of(dev, const struct pbl_gpio_port, dev);

  if (!pbl_device_is_ready(&port->dev)) {
    pbl_shell_error(sh, "port '%s' not ready", argv[1]);
    return -ENODEV;
  }

  if (pbl_shell_strtoul(argv[2], &pin) != 0 || pin >= port->num_pins) {
    pbl_shell_error(sh, "invalid pin '%s' (0-%u)", argv[2], port->num_pins - 1U);
    return -EINVAL;
  }

  *gpio = (struct pbl_gpio)PBL_GPIO(port, (uint8_t)pin, 0U);

  return 0;
}

static int prv_cmd_ports(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_gpio_port_class, dev) {
    const struct pbl_gpio_port *port = container_of(dev, const struct pbl_gpio_port, dev);

    pbl_shell_print(sh, "%s: %u pins%s%s%s", port->dev.name, port->num_pins,
                    (port->intc != nullptr) ? ", irq " : "",
                    (port->intc != nullptr) ? port->intc->dev.name : "",
                    pbl_device_is_ready(&port->dev) ? "" : " (not ready)");
  }

  return 0;
}

static int prv_cmd_get(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_gpio gpio;
  int ret;

  ret = prv_parse_pin(sh, argv, &gpio);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_gpio_get_raw(&gpio);
  if (ret < 0) {
    pbl_shell_error(sh, "read failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "%d", ret);

  return 0;
}

static int prv_cmd_set(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_gpio gpio;
  unsigned long level;
  int ret;

  ret = prv_parse_pin(sh, argv, &gpio);
  if (ret != 0) {
    return ret;
  }

  if (pbl_shell_strtoul(argv[3], &level) != 0 || level > 1UL) {
    pbl_shell_error(sh, "invalid level '%s' (0-1)", argv[3]);
    return -EINVAL;
  }

  ret = pbl_gpio_set_raw(&gpio, level != 0UL);
  if (ret != 0) {
    pbl_shell_error(sh, "write failed (%d)", ret);
  }

  return ret;
}

static const struct {
  const char *name;
  uint32_t flags;
} s_conf_flags[] = {
  {"in", PBL_GPIO_INPUT},         {"out", PBL_GPIO_OUTPUT}, {"low", PBL_GPIO_OUTPUT_LOW},
  {"high", PBL_GPIO_OUTPUT_HIGH}, {"pu", PBL_GPIO_PULL_UP}, {"pd", PBL_GPIO_PULL_DOWN},
  {"od", PBL_GPIO_OPEN_DRAIN},
};

static int prv_cmd_conf(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_gpio gpio;
  uint32_t flags = 0U;
  int ret;

  ret = prv_parse_pin(sh, argv, &gpio);
  if (ret != 0) {
    return ret;
  }

  for (size_t i = 3; i < argc; i++) {
    size_t j;

    for (j = 0; j < ARRAY_LENGTH(s_conf_flags); j++) {
      if (strcmp(argv[i], s_conf_flags[j].name) == 0) {
        flags |= s_conf_flags[j].flags;
        break;
      }
    }

    if (j == ARRAY_LENGTH(s_conf_flags)) {
      pbl_shell_error(sh, "unknown flag '%s' (in out low high pu pd od)", argv[i]);
      return -EINVAL;
    }
  }

  ret = pbl_gpio_configure(&gpio, flags);
  if (ret != 0) {
    pbl_shell_error(sh, "configure failed (%d)", ret);
  }

  return ret;
}

#ifdef CONFIG_INTC
static const struct {
  const char *name;
  uint8_t type;
} s_irq_types[] = {
  {"rising", PBL_IRQ_TYPE_EDGE_RISING}, {"falling", PBL_IRQ_TYPE_EDGE_FALLING},
  {"both", PBL_IRQ_TYPE_EDGE_BOTH},     {"high", PBL_IRQ_TYPE_LEVEL_HIGH},
  {"low", PBL_IRQ_TYPE_LEVEL_LOW},
};

static struct pbl_irq_spec s_watch;
static volatile uint32_t s_watch_count;

static void prv_watch_handler(void *arg) {
  s_watch_count++;

  // Level lines would fire continuously
  if (s_watch.type & (PBL_IRQ_TYPE_LEVEL_HIGH | PBL_IRQ_TYPE_LEVEL_LOW)) {
    (void)pbl_intc_disable(&s_watch);
  }
}

static int prv_watch_stop(const struct pbl_shell *sh) {
  int ret;

  if (s_watch.intc == nullptr) {
    return 0;
  }

  ret = pbl_intc_free(&s_watch);
  if (ret != 0) {
    pbl_shell_error(sh, "free failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "%s %u: %" PRIu32 " interrupts", s_watch.intc->dev.name, s_watch.hwirq,
                  s_watch_count);
  s_watch.intc = nullptr;

  return 0;
}

static int prv_cmd_irq(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_gpio gpio;
  struct pbl_irq_spec spec;
  size_t i;
  int ret;

  if (argc == 1) {
    if (s_watch.intc == nullptr) {
      pbl_shell_print(sh, "no pin watched");
    } else {
      pbl_shell_print(sh, "%s %u: %" PRIu32 " interrupts", s_watch.intc->dev.name, s_watch.hwirq,
                      s_watch_count);
    }
    return 0;
  }

  if (argc == 2 && strcmp(argv[1], "off") == 0) {
    return prv_watch_stop(sh);
  }

  if (argc != 4) {
    pbl_shell_error(sh, "usage: gpio irq [<port> <pin> <type> | off]");
    return -EINVAL;
  }

  ret = prv_parse_pin(sh, argv, &gpio);
  if (ret != 0) {
    return ret;
  }

  for (i = 0; i < ARRAY_LENGTH(s_irq_types); i++) {
    if (strcmp(argv[3], s_irq_types[i].name) == 0) {
      break;
    }
  }

  if (i == ARRAY_LENGTH(s_irq_types)) {
    pbl_shell_error(sh, "unknown type '%s' (rising falling both high low)", argv[3]);
    return -EINVAL;
  }

  ret = pbl_gpio_to_irq(&gpio, s_irq_types[i].type, &spec);
  if (ret != 0) {
    pbl_shell_error(sh, "port cannot interrupt");
    return ret;
  }

  ret = prv_watch_stop(sh);
  if (ret != 0) {
    return ret;
  }

  s_watch = spec;
  s_watch_count = 0U;

  ret = pbl_intc_request(&s_watch, prv_watch_handler, nullptr);
  if (ret != 0) {
    pbl_shell_error(sh, "request failed (%d)", ret);
    s_watch.intc = nullptr;
    return ret;
  }

  ret = pbl_intc_enable(&s_watch);
  if (ret != 0) {
    pbl_shell_error(sh, "enable failed (%d)", ret);
    (void)pbl_intc_free(&s_watch);
    s_watch.intc = nullptr;
  }

  return ret;
}
#endif

static const struct pbl_shell_cmd sub_gpio[] = {
  PBL_SHELL_CMD(ports, nullptr, "List the ports", prv_cmd_ports),
  PBL_SHELL_CMD_ARG(get, nullptr, "Read a pin's physical level <port> <pin>", prv_cmd_get, 3, 0),
  PBL_SHELL_CMD_ARG(set, nullptr, "Drive a pin's physical level <port> <pin> <0|1>", prv_cmd_set, 4,
                    0),
  PBL_SHELL_CMD_ARG(conf, nullptr, "Configure a pin <port> <pin> <in|out|low|high> [pu|pd|od]...",
                    prv_cmd_conf, 4, 3),
#ifdef CONFIG_INTC
  PBL_SHELL_CMD_ARG(irq, nullptr, "Count a pin's interrupts [<port> <pin> <type> | off]",
                    prv_cmd_irq, 1, 3),
#endif
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(gpio, sub_gpio, "GPIO ports", nullptr);
