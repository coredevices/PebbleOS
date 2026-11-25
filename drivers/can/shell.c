/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <pbl/drivers/can.h>
#include <pbl/kernel/msgq.h>
#include <pbl/kernel/time.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#define DUMP_FRAMES 16U

#define DUMP_SECONDS 10UL

#define SEND_TIMEOUT_MS 1000U

static PBL_MSGQ_DEFINE(s_dump_q, sizeof(struct pbl_can_frame), DUMP_FRAMES);

static const char *const s_states[] = {
  [PBL_CAN_STATE_STOPPED] = "stopped",
  [PBL_CAN_STATE_ERROR_ACTIVE] = "error-active",
  [PBL_CAN_STATE_ERROR_WARNING] = "error-warning",
  [PBL_CAN_STATE_ERROR_PASSIVE] = "error-passive",
  [PBL_CAN_STATE_BUS_OFF] = "bus-off",
};

static const struct {
  const char *name;
  uint32_t mode;
} s_modes[] = {
  {"loopback", PBL_CAN_MODE_LOOPBACK},
  {"listen-only", PBL_CAN_MODE_LISTEN_ONLY},
  {"non-iso", PBL_CAN_MODE_FD_NON_ISO},
  {"one-shot", PBL_CAN_MODE_ONE_SHOT},
  {"manual-recovery", PBL_CAN_MODE_MANUAL_RECOVERY},
};

static const char *const s_errors[PBL_CAN_ERROR_COUNT] = {
  [PBL_CAN_ERROR_BIT] = "bit",
  [PBL_CAN_ERROR_STUFF] = "stuff",
  [PBL_CAN_ERROR_FORM] = "form",
  [PBL_CAN_ERROR_ACK] = "ack",
  [PBL_CAN_ERROR_CRC] = "crc",
  [PBL_CAN_ERROR_ARBITRATION_LOST] = "arbitration-lost",
  [PBL_CAN_ERROR_RX_OVERRUN] = "rx-overrun",
};

static const struct pbl_can *prv_find(const struct pbl_shell *sh, const char *name) {
  const struct pbl_device *dev = pbl_device_find(&pbl_can_class, name);

  if (dev == nullptr) {
    pbl_shell_error(sh, "unknown controller '%s'", name);
    return nullptr;
  }

  return PBL_CONTAINER_OF(dev, const struct pbl_can, dev);
}

static int prv_hex(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  c = (char)tolower((unsigned char)c);
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return -1;
}

// <id>#<data>, <id>#R[<len>] or <id>##<flags><data>, data as hex bytes optionally split by dots;
// an identifier of 8 digits is an extended one
static int prv_parse_frame(const char *str, struct pbl_can_frame *frame) {
  const char *hash = strchr(str, '#');
  const char *p;
  size_t id_len;
  int v;

  memset(frame, 0, sizeof(*frame));

  if (hash == nullptr) {
    return -EINVAL;
  }

  id_len = (size_t)(hash - str);
  if (id_len != 3U && id_len != 8U) {
    return -EINVAL;
  }
  for (p = str; p < hash; p++) {
    v = prv_hex(*p);
    if (v < 0) {
      return -EINVAL;
    }
    frame->id = (frame->id << 4) | (uint32_t)v;
  }
  if (id_len == 8U) {
    frame->flags |= PBL_CAN_FRAME_IDE;
  }

  p = hash + 1;
  if (*p == 'R' || *p == 'r') {
    frame->flags |= PBL_CAN_FRAME_RTR;
    v = prv_hex(p[1]);
    if (v > (int)PBL_CAN_MAX_LEN) {
      return -EINVAL;
    }
    frame->len = (v > 0) ? (uint8_t)v : 0U;
    return 0;
  }

  if (*p == '#') {
    v = prv_hex(p[1]);
    if (v < 0) {
      return -EINVAL;
    }
    frame->flags |= PBL_CAN_FRAME_FDF;
    if ((v & 1) != 0) {
      frame->flags |= PBL_CAN_FRAME_BRS;
    }
    p += 2;
  }

  while (*p != '\0') {
    int hi, lo;

    if (*p == '.') {
      p++;
      continue;
    }

    hi = prv_hex(p[0]);
    lo = (hi >= 0) ? prv_hex(p[1]) : -1;
    if (lo < 0 || frame->len >= PBL_CAN_FD_MAX_LEN) {
      return -EINVAL;
    }
    frame->data[frame->len++] = (uint8_t)((hi << 4) | lo);
    p += 2;
  }

  return 0;
}

static void prv_print_frame(const struct pbl_shell *sh, const char *name,
                            const struct pbl_can_frame *frame) {
  char line[3U * PBL_CAN_FD_MAX_LEN + 1U];
  size_t pos = 0U;

  line[0] = '\0';
  if ((frame->flags & PBL_CAN_FRAME_RTR) == 0U) {
    for (uint8_t i = 0U; i < frame->len; i++) {
      pos += (size_t)snprintf(&line[pos], sizeof(line) - pos, " %02X", frame->data[i]);
    }
  }

  pbl_shell_print(sh, "%s  %*lX  [%u]%s%s%s%s%s", name,
                  ((frame->flags & PBL_CAN_FRAME_IDE) != 0U) ? 8 : 3, (unsigned long)frame->id,
                  frame->len, ((frame->flags & PBL_CAN_FRAME_FDF) != 0U) ? " fd" : "",
                  ((frame->flags & PBL_CAN_FRAME_BRS) != 0U) ? " brs" : "",
                  ((frame->flags & PBL_CAN_FRAME_ESI) != 0U) ? " esi" : "",
                  ((frame->flags & PBL_CAN_FRAME_RTR) != 0U) ? " remote" : "", line);
}

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_can_class, dev) {
    const struct pbl_can *can = PBL_CONTAINER_OF(dev, const struct pbl_can, dev);
    enum pbl_can_bus_state state;
    struct pbl_can_err_cnt cnt;
    struct pbl_can_config cfg;

    if (pbl_can_get_state(can, &state, &cnt) != 0) {
      pbl_shell_print(sh, "%s: not ready", pbl_device_name(dev));
      continue;
    }

    if (pbl_can_config_get(can, &cfg) != 0) {
      pbl_shell_print(sh, "%s: %s, not configured", pbl_device_name(dev), s_states[state]);
      continue;
    }

    pbl_shell_print(sh, "%s: %s, %lu bit/s, %lu bit/s data phase, mode 0x%lx, tec %u, rec %u",
                    pbl_device_name(dev), s_states[state], (unsigned long)cfg.bitrate,
                    (unsigned long)cfg.data_bitrate, (unsigned long)cfg.mode, cnt.tx, cnt.rx);
  }

  return 0;
}

static int prv_cmd_config(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_can *can = prv_find(sh, argv[1]);
  struct pbl_can_config cfg = {0};
  unsigned long val;
  size_t i = 3U;
  int ret;

  if (can == nullptr) {
    return -ENODEV;
  }

  if (pbl_shell_strtoul(argv[2], &val) != 0 || val == 0UL || val > UINT32_MAX) {
    pbl_shell_error(sh, "invalid bit rate '%s'", argv[2]);
    return -EINVAL;
  }
  cfg.bitrate = (uint32_t)val;

  if (i < argc && pbl_shell_strtoul(argv[i], &val) == 0) {
    if (val == 0UL || val > UINT32_MAX) {
      pbl_shell_error(sh, "invalid data bit rate '%s'", argv[i]);
      return -EINVAL;
    }
    cfg.data_bitrate = (uint32_t)val;
    cfg.mode |= PBL_CAN_MODE_FD;
    i++;
  }

  for (; i < argc; i++) {
    size_t m;

    for (m = 0U; m < PBL_ARRAY_SIZE(s_modes); m++) {
      if (strcmp(argv[i], s_modes[m].name) == 0) {
        cfg.mode |= s_modes[m].mode;
        break;
      }
    }
    if (m == PBL_ARRAY_SIZE(s_modes)) {
      pbl_shell_error(sh, "invalid mode '%s'", argv[i]);
      return -EINVAL;
    }
  }

  ret = pbl_can_configure(can, &cfg);
  if (ret != 0) {
    pbl_shell_error(sh, "configure failed (%d)", ret);
  }

  return ret;
}

static int prv_cmd_start(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_can *can = prv_find(sh, argv[1]);
  int ret;

  if (can == nullptr) {
    return -ENODEV;
  }

  ret = pbl_can_start(can);
  if (ret != 0) {
    pbl_shell_error(sh, "start failed (%d)", ret);
  }

  return ret;
}

static int prv_cmd_stop(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_can *can = prv_find(sh, argv[1]);

  if (can == nullptr) {
    return -ENODEV;
  }

  return pbl_can_stop(can);
}

static int prv_cmd_recover(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_can *can = prv_find(sh, argv[1]);
  int ret;

  if (can == nullptr) {
    return -ENODEV;
  }

  ret = pbl_can_recover(can);
  if (ret != 0) {
    pbl_shell_error(sh, "recover failed (%d)", ret);
  }

  return ret;
}

static int prv_cmd_stats(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_can *can = prv_find(sh, argv[1]);
  struct pbl_can_stats stats;
  int ret;

  if (can == nullptr) {
    return -ENODEV;
  }

  ret = pbl_can_get_stats(can, &stats);
  if (ret != 0) {
    pbl_shell_error(sh, "stats failed (%d)", ret);
    return ret;
  }

  for (size_t i = 0U; i < PBL_CAN_ERROR_COUNT; i++) {
    pbl_shell_print(sh, "%s: %lu", s_errors[i], (unsigned long)stats.errors[i]);
  }
  pbl_shell_print(sh, "bus-off: %lu", (unsigned long)stats.bus_off);

  return 0;
}

static int prv_cmd_filter(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_can *can = prv_find(sh, argv[1]);
  struct pbl_can_filter filter = {0};
  unsigned long id, mask;
  int ret;

  if (can == nullptr) {
    return -ENODEV;
  }

  if (pbl_shell_strtoul(argv[2], &id) != 0 || pbl_shell_strtoul(argv[3], &mask) != 0) {
    pbl_shell_error(sh, "invalid identifier or mask");
    return -EINVAL;
  }
  filter.id = (uint32_t)id;
  filter.mask = (uint32_t)mask;

  if (argc > 4) {
    if (strcmp(argv[4], "ext") != 0) {
      pbl_shell_error(sh, "invalid filter type '%s'", argv[4]);
      return -EINVAL;
    }
    filter.flags = PBL_CAN_FRAME_IDE;
  }

  ret = pbl_can_add_filter(can, &filter);
  if (ret < 0) {
    pbl_shell_error(sh, "adding the filter failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "filter %d", ret);

  return 0;
}

static int prv_cmd_unfilter(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_can *can = prv_find(sh, argv[1]);
  long idx;
  int ret;

  if (can == nullptr) {
    return -ENODEV;
  }

  if (pbl_shell_strtol(argv[2], &idx) != 0) {
    pbl_shell_error(sh, "invalid filter '%s'", argv[2]);
    return -EINVAL;
  }

  ret = pbl_can_remove_filter(can, (int)idx);
  if (ret != 0) {
    pbl_shell_error(sh, "removing the filter failed (%d)", ret);
  }

  return ret;
}

static int prv_cmd_send(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_can *can = prv_find(sh, argv[1]);
  struct pbl_can_frame frame;
  int ret;

  if (can == nullptr) {
    return -ENODEV;
  }

  if (prv_parse_frame(argv[2], &frame) != 0) {
    pbl_shell_error(sh, "invalid frame '%s'", argv[2]);
    return -EINVAL;
  }

  ret = pbl_can_send(can, &frame, PBL_MSEC(SEND_TIMEOUT_MS), nullptr, nullptr);
  if (ret != 0) {
    pbl_shell_error(sh, "send failed (%d)", ret);
  }

  return ret;
}

static void prv_dump_rx(const struct pbl_can *can, const struct pbl_can_frame *frame, void *arg) {
  (void)pbl_msgq_put(&s_dump_q, frame, PBL_NO_WAIT);
}

static int prv_cmd_dump(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_can *can = prv_find(sh, argv[1]);
  unsigned long seconds = DUMP_SECONDS;
  struct pbl_can_frame frame;
  uint64_t end;

  if (can == nullptr) {
    return -ENODEV;
  }

  if (argc > 2 && (pbl_shell_strtoul(argv[2], &seconds) != 0 || seconds == 0UL)) {
    pbl_shell_error(sh, "invalid duration '%s'", argv[2]);
    return -EINVAL;
  }

  pbl_msgq_purge(&s_dump_q);
  pbl_can_set_rx_callback(can, prv_dump_rx, nullptr);

  end = pbl_uptime_ms() + seconds * 1000UL;
  for (uint64_t now = pbl_uptime_ms(); now < end; now = pbl_uptime_ms()) {
    if (pbl_msgq_get(&s_dump_q, &frame, PBL_MSEC((uint32_t)(end - now))) == 0) {
      prv_print_frame(sh, argv[1], &frame);
    }
  }

  pbl_can_set_rx_callback(can, nullptr, nullptr);

  return 0;
}

static const struct pbl_shell_cmd sub_can[] = {
  PBL_SHELL_CMD(list, nullptr, "List the controllers", prv_cmd_list),
  PBL_SHELL_CMD_ARG(config, nullptr,
                    "Set the bit rates and mode, while stopped <can> <bitrate> [<data bitrate>] "
                    "[loopback|listen-only|non-iso|one-shot|manual-recovery]...",
                    prv_cmd_config, 3, 6),
  PBL_SHELL_CMD_ARG(start, nullptr, "Join the bus <can>", prv_cmd_start, 2, 0),
  PBL_SHELL_CMD_ARG(stop, nullptr, "Leave the bus <can>", prv_cmd_stop, 2, 0),
  PBL_SHELL_CMD_ARG(recover, nullptr, "Start the recovery from bus off <can>", prv_cmd_recover, 2,
                    0),
  PBL_SHELL_CMD_ARG(stats, nullptr, "Show the error counts <can>", prv_cmd_stats, 2, 0),
  PBL_SHELL_CMD_ARG(filter, nullptr, "Add a filter, while stopped <can> <id> <mask> [ext]",
                    prv_cmd_filter, 4, 1),
  PBL_SHELL_CMD_ARG(unfilter, nullptr, "Remove a filter, while stopped <can> <index>",
                    prv_cmd_unfilter, 3, 0),
  PBL_SHELL_CMD_ARG(send, nullptr,
                    "Send a frame <can> <id>#<data> | <id>#R[<len>] | <id>##<flags><data>",
                    prv_cmd_send, 3, 0),
  PBL_SHELL_CMD_ARG(dump, nullptr, "Show the frames received <can> [<seconds>]", prv_cmd_dump, 2,
                    1),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(can, sub_can, "CAN controllers", nullptr);
