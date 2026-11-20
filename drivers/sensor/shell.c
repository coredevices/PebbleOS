/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/drivers/sensor.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

static const char *const s_types[PBL_SENSOR_TYPE_COUNT] = {
  [PBL_SENSOR_ACCEL] = "accel", [PBL_SENSOR_GYRO] = "gyro",         [PBL_SENSOR_MAGN] = "magn",
  [PBL_SENSOR_TEMP] = "temp",   [PBL_SENSOR_PRESSURE] = "pressure",
};

static const char *const s_units[PBL_SENSOR_TYPE_COUNT] = {
  [PBL_SENSOR_ACCEL] = "ug", [PBL_SENSOR_GYRO] = "mdps",    [PBL_SENSOR_MAGN] = "nT",
  [PBL_SENSOR_TEMP] = "mC",  [PBL_SENSOR_PRESSURE] = "mPa",
};

static const char *const s_events[PBL_SENSOR_EVENT_COUNT] = {
  [PBL_SENSOR_EVENT_SHAKE] = "shake",
  [PBL_SENSOR_EVENT_DOUBLE_TAP] = "double-tap",
};

static const char *const s_attrs[] = {
  [PBL_SENSOR_ATTR_INTERVAL] = "interval",
  [PBL_SENSOR_ATTR_BATCH] = "batch",
  [PBL_SENSOR_ATTR_BATCH_MAX] = "batch-max",
  [PBL_SENSOR_ATTR_FULL_SCALE] = "full-scale",
  [PBL_SENSOR_ATTR_SHAKE_SENSITIVITY] = "shake-sensitivity",
};

static int prv_lookup(const char *const *names, size_t num, const char *name) {
  for (size_t i = 0U; i < num; i++) {
    if (names[i] != nullptr && strcmp(names[i], name) == 0) {
      return (int)i;
    }
  }

  return -ENOENT;
}

static const struct pbl_sensor *prv_find(const struct pbl_shell *sh, const char *name) {
  const struct pbl_device *dev;
  char *end;
  unsigned long idx = strtoul(name, &end, 10);
  bool by_idx = *name != '\0' && *end == '\0';
  unsigned long i = 0U;

  PBL_DEVICE_CLASS_FOREACH(&pbl_sensor_class, dev) {
    if (by_idx ? i == idx : strcmp(pbl_device_name(dev), name) == 0) {
      return PBL_CONTAINER_OF(dev, const struct pbl_sensor, dev);
    }
    i++;
  }

  pbl_shell_error(sh, "no sensor %s", name);
  return nullptr;
}

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;
  unsigned int i = 0U;

  PBL_DEVICE_CLASS_FOREACH(&pbl_sensor_class, dev) {
    const struct pbl_sensor *sensor = PBL_CONTAINER_OF(dev, const struct pbl_sensor, dev);
    char line[96];
    size_t len;

    len = (size_t)snprintf(line, sizeof(line), "%u: %s%s", i++, pbl_device_name(dev),
                           pbl_device_is_ready(dev) ? "" : " (not ready)");
    for (int t = 0; t < PBL_SENSOR_TYPE_COUNT && len < sizeof(line); t++) {
      if (pbl_sensor_has_type(sensor, t)) {
        len += (size_t)snprintf(&line[len], sizeof(line) - len, " %s", s_types[t]);
      }
    }
    for (int e = 0; e < PBL_SENSOR_EVENT_COUNT && len < sizeof(line); e++) {
      if (pbl_sensor_has_event(sensor, e)) {
        len += (size_t)snprintf(&line[len], sizeof(line) - len, " +%s", s_events[e]);
      }
    }

    pbl_shell_print(sh, "%s", line);
  }

  return 0;
}

static int prv_cmd_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_sensor *sensor = prv_find(sh, argv[1]);
  int32_t val[3];
  int ret;

  if (sensor == nullptr) {
    return -ENODEV;
  }

  for (int t = 0; t < PBL_SENSOR_TYPE_COUNT; t++) {
    if (!pbl_sensor_has_type(sensor, t) || (argc > 2U && strcmp(argv[2], s_types[t]) != 0)) {
      continue;
    }

    ret = pbl_sensor_read(sensor, t, val, nullptr);
    if (ret != 0) {
      pbl_shell_error(sh, "%s: read failed (%d)", s_types[t], ret);
    } else if (pbl_sensor_num_values(t) == 3U) {
      pbl_shell_print(sh, "%s: x=%" PRId32 " y=%" PRId32 " z=%" PRId32 " %s", s_types[t], val[0],
                      val[1], val[2], s_units[t]);
    } else {
      pbl_shell_print(sh, "%s: %" PRId32 " %s", s_types[t], val[0], s_units[t]);
    }
  }

  return 0;
}

static int prv_cmd_attr(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_sensor *sensor = prv_find(sh, argv[1]);
  int type = prv_lookup(s_types, PBL_ARRAY_SIZE(s_types), argv[2]);
  int attr = prv_lookup(s_attrs, PBL_ARRAY_SIZE(s_attrs), argv[3]);
  int32_t val;
  int ret;

  if (sensor == nullptr) {
    return -ENODEV;
  }

  if (type < 0 || attr < 0) {
    pbl_shell_error(sh, "unknown type or attribute");
    return -EINVAL;
  }

  if (argc > 4U) {
    val = (int32_t)strtol(argv[4], nullptr, 0);
    ret = pbl_sensor_attr_set(sensor, type, attr, &val);
  } else {
    ret = pbl_sensor_attr_get(sensor, type, attr, &val);
  }

  if (ret != 0) {
    pbl_shell_error(sh, "failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "%" PRId32, val);

  return 0;
}

static const struct pbl_shell_cmd sub_sensor[] = {
  PBL_SHELL_CMD(list, nullptr, "List the sensors", prv_cmd_list),
  PBL_SHELL_CMD_ARG(read, nullptr, "Read a sensor: read <sensor> [type]", prv_cmd_read, 2, 1),
  PBL_SHELL_CMD_ARG(attr, nullptr, "Get or set an attribute: attr <sensor> <type> <attr> [value]",
                    prv_cmd_attr, 4, 1),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(sensor, sub_sensor, "Sensors", nullptr);
