/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/i2c.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>

#define ADDR_FIRST 0x08U
#define ADDR_LAST  0x77U
#define MAX_BYTES  16U

static int prv_parse_dev(const struct pbl_shell *sh, char **argv, struct pbl_i2c_dev *dev) {
  const struct pbl_device *bus_dev = pbl_device_find(&pbl_i2c_bus_class, argv[1]);
  unsigned long addr;

  if (bus_dev == nullptr) {
    pbl_shell_error(sh, "unknown bus '%s'", argv[1]);
    return -ENODEV;
  }

  if (pbl_shell_strtoul(argv[2], &addr) != 0 || addr > 0x7fUL) {
    pbl_shell_error(sh, "invalid address '%s'", argv[2]);
    return -EINVAL;
  }

  *dev = (struct pbl_i2c_dev)PBL_I2C_DEV(container_of(bus_dev, const struct pbl_i2c_bus, dev),
                                         (uint16_t)addr);

  return 0;
}

static int prv_parse_byte(const struct pbl_shell *sh, const char *arg, uint8_t *val) {
  unsigned long v;

  if (pbl_shell_strtoul(arg, &v) != 0 || v > 0xffUL) {
    pbl_shell_error(sh, "invalid byte '%s'", arg);
    return -EINVAL;
  }

  *val = (uint8_t)v;

  return 0;
}

static int prv_cmd_buses(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_i2c_bus_class, dev) {
    pbl_shell_print(sh, "%s%s", dev->name, pbl_device_is_ready(dev) ? "" : " (not ready)");
  }

  return 0;
}

static int prv_cmd_scan(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *bus_dev = pbl_device_find(&pbl_i2c_bus_class, argv[1]);
  unsigned int found = 0U;

  if (bus_dev == nullptr) {
    pbl_shell_error(sh, "unknown bus '%s'", argv[1]);
    return -ENODEV;
  }

  for (uint16_t addr = ADDR_FIRST; addr <= ADDR_LAST; addr++) {
    struct pbl_i2c_dev dev =
        PBL_I2C_DEV(container_of(bus_dev, const struct pbl_i2c_bus, dev), addr);

    if (pbl_i2c_write(&dev, nullptr, 0U) == 0) {
      pbl_shell_print(sh, "0x%02x", addr);
      found++;
    }
  }

  pbl_shell_print(sh, "%u devices", found);

  return 0;
}

static int prv_cmd_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_i2c_dev dev;
  uint8_t buf[MAX_BYTES];
  unsigned long len = 1UL;
  uint8_t reg;
  int ret;

  ret = prv_parse_dev(sh, argv, &dev);
  if (ret != 0) {
    return ret;
  }

  ret = prv_parse_byte(sh, argv[3], &reg);
  if (ret != 0) {
    return ret;
  }

  if (argc > 4U && (pbl_shell_strtoul(argv[4], &len) != 0 || len == 0UL || len > MAX_BYTES)) {
    pbl_shell_error(sh, "invalid length '%s' (1-%u)", argv[4], MAX_BYTES);
    return -EINVAL;
  }

  ret = pbl_i2c_burst_read(&dev, reg, buf, len);
  if (ret != 0) {
    pbl_shell_error(sh, "read failed (%d)", ret);
    return ret;
  }

  pbl_shell_hexdump(sh, buf, len);

  return 0;
}

static int prv_cmd_write(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_i2c_dev dev;
  uint8_t buf[MAX_BYTES];
  uint8_t reg;
  int ret;

  ret = prv_parse_dev(sh, argv, &dev);
  if (ret != 0) {
    return ret;
  }

  ret = prv_parse_byte(sh, argv[3], &reg);
  if (ret != 0) {
    return ret;
  }

  for (size_t i = 4U; i < argc; i++) {
    ret = prv_parse_byte(sh, argv[i], &buf[i - 4U]);
    if (ret != 0) {
      return ret;
    }
  }

  ret = pbl_i2c_burst_write(&dev, reg, buf, argc - 4U);
  if (ret != 0) {
    pbl_shell_error(sh, "write failed (%d)", ret);
  }

  return ret;
}

static const struct pbl_shell_cmd sub_i2c[] = {
  PBL_SHELL_CMD(buses, nullptr, "List the buses", prv_cmd_buses),
  PBL_SHELL_CMD_ARG(scan, nullptr, "Probe every address with an empty write <bus>", prv_cmd_scan, 2,
                    0),
  PBL_SHELL_CMD_ARG(read, nullptr, "Read registers <bus> <addr> <reg> [len]", prv_cmd_read, 4, 1),
  PBL_SHELL_CMD_ARG(write, nullptr, "Write registers <bus> <addr> <reg> <byte>...", prv_cmd_write,
                    5, MAX_BYTES - 1U),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(i2c, sub_i2c, "I2C buses", nullptr);
