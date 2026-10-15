/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>

#include <pbl/crc/crc.h>
#include <pbl/drivers/crc.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/types.h>
#include <pbl/shell/shell.h>

#define CRC_BUF_LEN     1024UL
#define CRC_SW_CHUNK    16U
#define CRC_BENCH_RUNS  1000U
#define CRC_DEFAULT_LEN 1024UL

static uint8_t s_buf[CRC_BUF_LEN + 1U];

static void prv_fill(void) {
  uint32_t x = 0x2545f491U;

  for (size_t i = 0; i < sizeof(s_buf); i++) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_buf[i] = (uint8_t)x;
  }
}

static int prv_parse_len(const struct pbl_shell *sh, size_t argc, char **argv, size_t *len) {
  unsigned long val = CRC_DEFAULT_LEN;

  if (argc > 1 && (pbl_shell_strtoul(argv[1], &val) != 0 || val == 0UL || val > CRC_BUF_LEN)) {
    pbl_shell_error(sh, "invalid length '%s' (1-%lu)", argv[1], CRC_BUF_LEN);
    return -EINVAL;
  }

  *len = val;

  return 0;
}

static uint32_t prv_crc32_hw(const uint8_t *data, size_t len) {
  uint32_t crc = 0U;
  size_t done = pbl_crc_hw_crc32(&crc, data, len);

  return pbl_crc32(crc, &data[done], len - done);
}

static uint32_t prv_crc32_sw(const uint8_t *data, size_t len) {
  uint32_t crc = 0U;

  for (size_t i = 0; i < len; i += CRC_SW_CHUNK) {
    crc = pbl_crc32(crc, &data[i], (len - i < CRC_SW_CHUNK) ? len - i : CRC_SW_CHUNK);
  }

  return crc;
}

static uint32_t prv_legacy_hw(const uint8_t *data, size_t len) {
  return pbl_crc32_legacy(data, len);
}

static uint32_t prv_legacy_sw(const uint8_t *data, size_t len) {
  struct pbl_crc32_legacy ctx;

  pbl_crc32_legacy_init(&ctx);
  for (size_t i = 0; i < len; i += CRC_SW_CHUNK) {
    pbl_crc32_legacy_update(&ctx, &data[i], (len - i < CRC_SW_CHUNK) ? len - i : CRC_SW_CHUNK);
  }

  return pbl_crc32_legacy_finish(&ctx);
}

static int prv_check(const struct pbl_shell *sh, const char *name,
                     uint32_t (*hw)(const uint8_t *, size_t),
                     uint32_t (*sw)(const uint8_t *, size_t), const uint8_t *data, size_t len) {
  uint32_t h = hw(data, len);
  uint32_t s = sw(data, len);

  pbl_shell_print(sh, "%s: hw 0x%08" PRIx32 " sw 0x%08" PRIx32 " %s", name, h, s,
                  (h == s) ? "ok" : "MISMATCH");

  return (h == s) ? 0 : -EIO;
}

static int prv_cmd_check(const struct pbl_shell *sh, size_t argc, char **argv) {
  size_t len;
  int ret;

  ret = prv_parse_len(sh, argc, argv, &len);
  if (ret != 0) {
    return ret;
  }

  prv_fill();

  ret = prv_check(sh, "crc32", prv_crc32_hw, prv_crc32_sw, s_buf, len);
  if (ret != 0) {
    return ret;
  }

  ret = prv_check(sh, "crc32 unaligned", prv_crc32_hw, prv_crc32_sw, &s_buf[1], len);
  if (ret != 0) {
    return ret;
  }

  ret = prv_check(sh, "legacy", prv_legacy_hw, prv_legacy_sw, s_buf, len);
  if (ret != 0) {
    return ret;
  }

  return prv_check(sh, "legacy unaligned", prv_legacy_hw, prv_legacy_sw, &s_buf[1], len);
}

static void prv_bench(const struct pbl_shell *sh, const char *name,
                      uint32_t (*fn)(const uint8_t *, size_t), size_t len) {
  pbl_tick_t start = pbl_uptime_ticks();

  for (unsigned int i = 0U; i < CRC_BENCH_RUNS; i++) {
    (void)fn(s_buf, len);
  }

  pbl_shell_print(sh, "%s: %" PRIu32 " ms for %u x %zu bytes", name,
                  pbl_ticks_to_ms(pbl_uptime_ticks() - start), CRC_BENCH_RUNS, len);
}

static int prv_cmd_bench(const struct pbl_shell *sh, size_t argc, char **argv) {
  size_t len;
  int ret;

  ret = prv_parse_len(sh, argc, argv, &len);
  if (ret != 0) {
    return ret;
  }

  prv_fill();
  prv_bench(sh, "crc32 hw", prv_crc32_hw, len);
  prv_bench(sh, "crc32 sw", prv_crc32_sw, len);
  prv_bench(sh, "legacy hw", prv_legacy_hw, len);
  prv_bench(sh, "legacy sw", prv_legacy_sw, len);

  return 0;
}

static const struct pbl_shell_cmd sub_crc[] = {
  PBL_SHELL_CMD_ARG(check, nullptr, "Compare hardware and software CRCs [len]", prv_cmd_check, 1,
                    1),
  PBL_SHELL_CMD_ARG(bench, nullptr, "Time hardware and software CRCs [len]", prv_cmd_bench, 1, 1),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(crc, sub_crc, "CRC unit", nullptr);
