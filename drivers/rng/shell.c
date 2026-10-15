/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>

#include <pbl/drivers/rng.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/types.h>
#include <pbl/shell/shell.h>

#define RNG_GET_MAX   64UL
#define RNG_STATS_DEF 1000UL

static int prv_parse_count(const struct pbl_shell *sh, size_t argc, char **argv, unsigned long def,
                           unsigned long max, unsigned long *count) {
  *count = def;
  if (argc < 2) {
    return 0;
  }

  if (pbl_shell_strtoul(argv[1], count) != 0 || *count == 0UL || *count > max) {
    pbl_shell_error(sh, "invalid count '%s' (1-%lu)", argv[1], max);
    return -EINVAL;
  }

  return 0;
}

static int prv_cmd_get(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long count;
  uint32_t val;
  int ret;

  ret = prv_parse_count(sh, argc, argv, 1UL, RNG_GET_MAX, &count);
  if (ret != 0) {
    return ret;
  }

  for (unsigned long i = 0UL; i < count; i++) {
    if (!rng_rand(&val)) {
      pbl_shell_error(sh, "generation failed");
      return -EIO;
    }

    pbl_shell_print(sh, "0x%08" PRIx32, val);
  }

  return 0;
}

static int prv_cmd_stats(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long count;
  uint32_t val, prev = 0U, fails = 0U, repeats = 0U;
  uint64_t ones = 0U;
  pbl_tick_t start;
  int ret;

  ret = prv_parse_count(sh, argc, argv, RNG_STATS_DEF, UINT32_MAX / 32UL, &count);
  if (ret != 0) {
    return ret;
  }

  start = pbl_uptime_ticks();
  for (unsigned long i = 0UL; i < count; i++) {
    if (!rng_rand(&val)) {
      fails++;
      continue;
    }

    if (i > 0UL && val == prev) {
      repeats++;
    }
    ones += (uint64_t)__builtin_popcount(val);
    prev = val;
  }

  pbl_shell_print(sh, "words: %lu, failed: %" PRIu32 ", repeated: %" PRIu32, count, fails, repeats);
  if (fails < count) {
    uint64_t bits = (uint64_t)(count - fails) * 32U;

    pbl_shell_print(sh, "ones: %" PRIu32 " per mille", (uint32_t)(ones * 1000U / bits));
  }
  pbl_shell_print(sh, "time: %" PRIu32 " ms", pbl_ticks_to_ms(pbl_uptime_ticks() - start));

  return 0;
}

static const struct pbl_shell_cmd sub_rng[] = {
  PBL_SHELL_CMD_ARG(get, nullptr, "Print random words [count]", prv_cmd_get, 1, 1),
  PBL_SHELL_CMD_ARG(stats, nullptr, "Generate words and show bit statistics [count]", prv_cmd_stats,
                    1, 1),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(rng, sub_rng, "Random number generator", nullptr);
