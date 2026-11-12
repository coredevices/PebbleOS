/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>

#include <pbl/drivers/adc.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_adc_class, dev) {
    const struct pbl_adc *adc = container_of(dev, const struct pbl_adc, dev);

    pbl_shell_print(sh, "%s: %u channels%s", adc->dev.name, adc->num_channels,
                    pbl_device_is_ready(&adc->dev) ? "" : " (not ready)");
  }

  return 0;
}

static int prv_cmd_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev = pbl_device_find(&pbl_adc_class, argv[1]);
  struct pbl_adc_channel channel;
  unsigned long id;
  int32_t uv;
  int ret;

  if (dev == nullptr) {
    pbl_shell_error(sh, "unknown ADC '%s'", argv[1]);
    return -ENODEV;
  }

  if (pbl_shell_strtoul(argv[2], &id) != 0) {
    pbl_shell_error(sh, "invalid channel '%s'", argv[2]);
    return -EINVAL;
  }

  channel = (struct pbl_adc_channel)PBL_ADC_CHANNEL(container_of(dev, const struct pbl_adc, dev),
                                                    (uint32_t)id);
  ret = pbl_adc_read(&channel, &uv);
  if (ret != 0) {
    pbl_shell_error(sh, "read failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "%" PRId32 " uV", uv);

  return 0;
}

static const struct pbl_shell_cmd sub_adc[] = {
  PBL_SHELL_CMD(list, nullptr, "List the ADCs", prv_cmd_list),
  PBL_SHELL_CMD_ARG(read, nullptr, "Convert a channel once <adc> <channel>", prv_cmd_read, 3, 0),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(adc, sub_adc, "Analog-to-digital converters", nullptr);
