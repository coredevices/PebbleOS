/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/spi.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>

#define MAX_BYTES 16U

static int prv_cmd_buses(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_spi_bus_class, dev) {
    const struct pbl_spi_bus *bus = container_of(dev, const struct pbl_spi_bus, dev);

    pbl_shell_print(sh, "%s: %u chip selects%s", dev->name, bus->num_cs,
                    pbl_device_is_ready(dev) ? "" : " (not ready)");
  }

  return 0;
}

static int prv_cmd_xfer(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *bus_dev = pbl_device_find(&pbl_spi_bus_class, argv[1]);
  struct pbl_spi_dev dev;
  struct pbl_spi_xfer xfer;
  uint8_t buf[MAX_BYTES];
  unsigned long cs;
  unsigned long hz;
  unsigned long mode;
  int ret;

  if (bus_dev == nullptr) {
    pbl_shell_error(sh, "unknown bus '%s'", argv[1]);
    return -ENODEV;
  }

  if (pbl_shell_strtoul(argv[2], &cs) != 0 || cs > UINT8_MAX) {
    pbl_shell_error(sh, "invalid chip select '%s'", argv[2]);
    return -EINVAL;
  }

  if (pbl_shell_strtoul(argv[3], &hz) != 0 || hz == 0UL) {
    pbl_shell_error(sh, "invalid frequency '%s'", argv[3]);
    return -EINVAL;
  }

  if (pbl_shell_strtoul(argv[4], &mode) != 0 ||
      (mode & ~(unsigned long)(PBL_SPI_MODE_3 | PBL_SPI_CS_HIGH | PBL_SPI_LSB_FIRST)) != 0UL) {
    pbl_shell_error(sh, "invalid mode '%s'", argv[4]);
    return -EINVAL;
  }

  for (size_t i = 5U; i < argc; i++) {
    unsigned long v;

    if (pbl_shell_strtoul(argv[i], &v) != 0 || v > 0xffUL) {
      pbl_shell_error(sh, "invalid byte '%s'", argv[i]);
      return -EINVAL;
    }

    buf[i - 5U] = (uint8_t)v;
  }

  dev = (struct pbl_spi_dev)PBL_SPI_DEV(container_of(bus_dev, const struct pbl_spi_bus, dev),
                                        (uint8_t)cs, (uint32_t)hz, (uint32_t)mode);
  ret = pbl_spi_setup(&dev);
  if (ret != 0) {
    pbl_shell_error(sh, "setup failed (%d)", ret);
    return ret;
  }

  xfer = (struct pbl_spi_xfer){.tx_buf = buf, .rx_buf = buf, .len = argc - 5U};

  ret = pbl_spi_transfer(&dev, &xfer, 1U);
  if (ret != 0) {
    pbl_shell_error(sh, "transfer failed (%d)", ret);
    return ret;
  }

  pbl_shell_hexdump(sh, buf, xfer.len);

  return 0;
}

static const struct pbl_shell_cmd sub_spi[] = {
  PBL_SHELL_CMD(buses, nullptr, "List the buses", prv_cmd_buses),
  PBL_SHELL_CMD_ARG(xfer, nullptr,
                    "Write bytes and print the bytes read <bus> <cs> <hz> <mode> <byte>...",
                    prv_cmd_xfer, 6, MAX_BYTES - 1U),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(spi, sub_spi, "SPI buses", nullptr);
