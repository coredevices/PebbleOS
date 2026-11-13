/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>

#include <pbl/drivers/ram.h>
#include <pbl/kernel/scratch.h>
#include <pbl/shell/shell.h>

static int prv_cmd_info(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_ram_state *st = EXTRAM->state;
  struct pbl_scratch_stats stats;
  uintptr_t base;
  size_t size;

  pbl_ram_get_region(EXTRAM, &base, &size);
  pbl_scratch_get_stats(&stats);
  pbl_shell_print(sh,
                  "%s: %" PRIu32 " bytes at 0x%08" PRIxPTR ", %s, %" PRIu32
                  " users, powered on %" PRIu32 " times",
                  EXTRAM->dev.name, (uint32_t)size, base, st->on ? "on" : "off", st->users,
                  st->power_cycles);
  pbl_shell_print(sh,
                  "scratch heap: %" PRIu32 " of %" PRIu32 " bytes in %u allocations, peak %" PRIu32
                  ", largest free %" PRIu32,
                  (uint32_t)stats.used, (uint32_t)stats.size, stats.allocations,
                  (uint32_t)stats.peak, (uint32_t)stats.max_free);

  return 0;
}

static uint32_t *s_held;
static size_t s_held_words;

static uint32_t prv_pattern(size_t i, uint32_t seed) {
  return ((uint32_t)i * 2654435761U) ^ seed;
}

static int prv_fill(const struct pbl_shell *sh, size_t argc, char **argv, uint32_t **buf,
                    size_t *words) {
  unsigned long kib = 1024UL;

  if (argc > 1 && (pbl_shell_strtoul(argv[1], &kib) != 0 || kib == 0UL)) {
    pbl_shell_error(sh, "invalid size '%s'", argv[1]);
    return -EINVAL;
  }

  *buf = pbl_scratch_malloc(kib * 1024UL);
  if (*buf == NULL) {
    pbl_shell_error(sh, "no room for %lu KiB", kib);
    return -ENOMEM;
  }

  *words = kib * 1024UL / sizeof(uint32_t);
  for (size_t i = 0U; i < *words; i++) {
    (*buf)[i] = prv_pattern(i, (uint32_t)(uintptr_t)*buf);
  }

  return 0;
}

static int prv_verify(const struct pbl_shell *sh, uint32_t *buf, size_t words) {
  size_t bad = 0U;

  for (size_t i = 0U; i < words; i++) {
    if (buf[i] != prv_pattern(i, (uint32_t)(uintptr_t)buf)) {
      bad++;
    }
  }
  pbl_scratch_free(buf);

  if (bad > 0U) {
    pbl_shell_error(sh, "%u of %u words wrong", (unsigned int)bad, (unsigned int)words);
    return -EIO;
  }

  pbl_shell_print(sh, "%u KiB ok", (unsigned int)(words * sizeof(uint32_t) / 1024U));
  return 0;
}

static int prv_cmd_check(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t *buf;
  size_t words;
  int ret;

  ret = prv_fill(sh, argc, argv, &buf, &words);
  if (ret != 0) {
    return ret;
  }

  return prv_verify(sh, buf, words);
}

// Keeps a buffer allocated, so that deep sleep retains it, until "release" checks it
static int prv_cmd_hold(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (s_held != NULL) {
    pbl_shell_error(sh, "already holding a buffer");
    return -EBUSY;
  }

  return prv_fill(sh, argc, argv, &s_held, &s_held_words);
}

static int prv_cmd_release(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t *buf = s_held;

  if (buf == NULL) {
    pbl_shell_error(sh, "no buffer held");
    return -ENOENT;
  }
  s_held = NULL;

  return prv_verify(sh, buf, s_held_words);
}

static const struct pbl_shell_cmd sub_ram[] = {
  PBL_SHELL_CMD(info, nullptr, "Show the power state and the scratch heap use", prv_cmd_info),
  PBL_SHELL_CMD(check, nullptr, "Write and check a buffer [KiB]", prv_cmd_check),
  PBL_SHELL_CMD(hold, nullptr, "Write a buffer and keep it [KiB]", prv_cmd_hold),
  PBL_SHELL_CMD(release, nullptr, "Check the buffer kept by hold, and free it", prv_cmd_release),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(ram, sub_ram, "External RAM", nullptr);
