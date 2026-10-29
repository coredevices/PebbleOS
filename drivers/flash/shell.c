/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <pbl/drivers/flash.h>
#include <pbl/drivers/rtc.h>
#include <pbl/logging/logging.h>
#include <pbl/services/system_task.h>
#include <pbl/shell/shell.h>
#include <pbl/task_wdt/task_wdt.h>
#include <pbl/util/math.h>
#include <pbl/util/rand32.h>
#include <pbl/util/size.h>

#include <flash_region/flash_region.h>
#include <kernel/pbl_malloc.h>

#define ERASED_BYTE 0xFFU

PBL_SHELL_SUBCMD_SET_CREATE(sub_flash);
PBL_SHELL_CMD_REGISTER(flash, sub_flash, "Flash", nullptr);

static int prv_parse_u32(const struct pbl_shell *sh, const char *str, uint32_t *out) {
  unsigned long val;

  if (pbl_shell_strtoul(str, &val) != 0 || val > UINT32_MAX) {
    pbl_shell_error(sh, "invalid value '%s'", str);
    return -EINVAL;
  }

  *out = val;

  return 0;
}

static int prv_parse_range(const struct pbl_shell *sh, const char *addr_str, const char *len_str,
                           uint32_t *addr, uint32_t *len) {
  int ret;

  ret = prv_parse_u32(sh, addr_str, addr);
  if (ret != 0) {
    return ret;
  }

  ret = prv_parse_u32(sh, len_str, len);
  if (ret != 0) {
    return ret;
  }

  if (*len == 0U) {
    pbl_shell_error(sh, "invalid length '%s'", len_str);
    return -EINVAL;
  }

  return 0;
}

static int prv_cmd_info(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_flash_geometry *geo = FLASH->geometry;

  pbl_shell_print(sh, "%s: 0x%08" PRIx32 ", %" PRIu32 " bytes%s", FLASH->dev.name, FLASH->base,
                  geo->size, pbl_device_is_ready(&FLASH->dev) ? "" : " (not ready)");
  pbl_shell_print(sh, "page %" PRIu32 ", sector %" PRIu32 ", subsector %" PRIu32, geo->page_size,
                  geo->sector_size, geo->subsector_size);

  return 0;
}

static int prv_cmd_erase(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t addr;
  uint32_t len;
  int ret;

  ret = prv_parse_range(sh, argv[1], argv[2], &addr, &len);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_flash_erase(FLASH, addr, len);
  if (ret != 0) {
    pbl_shell_error(sh, "erase failed (%d)", ret);
    return ret;
  }

  return 0;
}

static int prv_cmd_dump(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint8_t buf[128];
  uint32_t addr;
  uint32_t len;
  int ret;

  ret = prv_parse_range(sh, argv[1], argv[2], &addr, &len);
  if (ret != 0) {
    return ret;
  }

  while (len > 0U) {
    uint32_t chunk = MIN(len, sizeof(buf));

    ret = pbl_flash_read(FLASH, addr, buf, chunk);
    if (ret != 0) {
      pbl_shell_error(sh, "read failed (%d)", ret);
      return ret;
    }

    pbl_shell_print(sh, "data at address 0x%" PRIx32, addr);
    pbl_shell_hexdump(sh, buf, chunk);

    addr += chunk;
    len -= chunk;
    system_task_watchdog_feed();
  }

  return 0;
}

static int prv_cmd_crc(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t addr;
  uint32_t len;
  int ret;

  ret = prv_parse_range(sh, argv[1], argv[2], &addr, &len);
  if (ret != 0) {
    return ret;
  }

  pbl_shell_print(sh, "CRC: %" PRIx32, pbl_flash_crc32_legacy(FLASH, addr, len));

  return 0;
}

static int prv_cmd_unprotect(const struct pbl_shell *sh, size_t argc, char **argv) {
  return pbl_flash_unprotect(FLASH);
}

PBL_SHELL_SUBCMD_ADD(sub_flash, info, nullptr, "Show the flash", prv_cmd_info, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, erase, nullptr, "Erase the subsectors covering <addr> <len>",
                     prv_cmd_erase, 3, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, crc, nullptr, "Legacy CRC of <addr> <len>", prv_cmd_crc, 3, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, dump, nullptr, "Hex dump <addr> <len>", prv_cmd_dump, 3, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, unprotect, nullptr, "Remove the flash write protection",
                     prv_cmd_unprotect, 0, 0);

#ifdef CONFIG_RECOVERY_FW
#define MAX_READ_SIZE     1024U
#define FILL_CHUNK_SIZE   64U
#define VALIDATE_BUF_SIZE 256U
// Above 128 bytes reads take another path on some controllers
#define VALIDATE_MIXED_READ_SIZE 130U
#define VALIDATE_MIXED_LEN       1000U

static int prv_cmd_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t addr;
  uint32_t len;
  uint8_t *buf;
  int ret;

  ret = prv_parse_range(sh, argv[1], argv[2], &addr, &len);
  if (ret != 0) {
    return ret;
  }

  buf = kernel_malloc(MIN(MAX_READ_SIZE, len));
  if (buf == nullptr) {
    pbl_shell_error(sh, "unable to allocate read buffer");
    return -ENOMEM;
  }

  while (len > 0U) {
    uint32_t chunk = MIN(len, MAX_READ_SIZE);

    ret = pbl_flash_read(FLASH, addr, buf, chunk);
    if (ret != 0) {
      break;
    }

    // raw bytes, not text
    for (uint32_t i = 0U; i < chunk; i++) {
      pbl_shell_fprintf(sh, "%c", buf[i]);
    }

    addr += chunk;
    len -= chunk;
  }

  kernel_free(buf);

  return ret;
}

static int prv_cmd_fill(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint8_t chunk_buf[FILL_CHUNK_SIZE];
  uint32_t addr;
  uint32_t len;
  uint32_t value;
  int ret;

  ret = prv_parse_range(sh, argv[1], argv[2], &addr, &len);
  if (ret != 0) {
    return ret;
  }

  ret = prv_parse_u32(sh, argv[3], &value);
  if (ret != 0) {
    return ret;
  }

  for (uint32_t i = 0U; i < FILL_CHUNK_SIZE; i++) {
    chunk_buf[i] = (uint8_t)(value + i);
  }

  while (len > 0U) {
    uint32_t chunk = MIN(len, FILL_CHUNK_SIZE);

    ret = pbl_flash_write(FLASH, addr, chunk_buf, chunk);
    if (ret != 0) {
      pbl_shell_error(sh, "write failed (%d)", ret);
      return ret;
    }

    len -= chunk;
    addr += chunk;
  }

  return 0;
}

static int prv_validate_erase(const struct pbl_shell *sh, uint32_t addr, uint32_t len) {
  int ret;

  ret = pbl_flash_erase(FLASH, addr, len);
  if (ret != 0) {
    pbl_shell_error(sh, "erase failed (%d)", ret);
    return ret;
  }

  if (!pbl_flash_is_erased(FLASH, addr, len)) {
    pbl_shell_error(sh, "not erased");
    return -EIO;
  }

  return 0;
}

static int prv_cmd_validate(const struct pbl_shell *sh, size_t argc, char **argv) {
  const uint32_t test_addr = FLASH_REGION_FIRMWARE_DEST_BEGIN;
  const uint32_t test_len = FLASH->geometry->sector_size;
  uint8_t buf[VALIDATE_BUF_SIZE];
  int ret;

  ret = prv_validate_erase(sh, test_addr, test_len);
  if (ret != 0) {
    return ret;
  }

  for (uint32_t i = 0U; i < sizeof(buf); i++) {
    buf[i] = (uint8_t)i;
  }

  for (uint32_t offset = 0U; offset < test_len; offset += sizeof(buf)) {
    ret = pbl_flash_write(FLASH, test_addr + offset, buf, sizeof(buf));
    if (ret != 0) {
      pbl_shell_error(sh, "write failed (%d)", ret);
      return ret;
    }
  }

  for (uint32_t offset = 0U; offset < test_len; offset += sizeof(buf)) {
    memset(buf, 0, sizeof(buf));
    ret = pbl_flash_read(FLASH, test_addr + offset, buf, sizeof(buf));
    if (ret != 0) {
      pbl_shell_error(sh, "read failed (%d)", ret);
      return ret;
    }

    for (uint32_t i = 0U; i < sizeof(buf); i++) {
      if (buf[i] != (uint8_t)i) {
        pbl_shell_error(sh, "incorrect value at 0x%" PRIx32, test_addr + offset + i);
        return -EIO;
      }
    }
  }

  // Mixing single byte reads with longer ones has caused issues before
  for (uint32_t offset = 0U; offset < VALIDATE_MIXED_LEN; offset++) {
    const uint32_t pre_addr = test_addr + offset - MIN(offset, 1U);
    const size_t len = MIN(VALIDATE_MIXED_READ_SIZE, VALIDATE_MIXED_LEN - offset);
    uint8_t pre_byte;

    (void)pbl_flash_read(FLASH, pre_addr, &pre_byte, sizeof(pre_byte));

    memset(buf, 0, sizeof(buf));
    (void)pbl_flash_read(FLASH, test_addr + offset, buf, len);
    for (size_t i = 0U; i < len; i++) {
      if (buf[i] != (uint8_t)(offset + i)) {
        pbl_shell_error(sh, "failed at offset %" PRIu32 ", got %u, wanted %u", offset, buf[i],
                        (uint8_t)(offset + i));
        return -EIO;
      }
    }
  }

  ret = prv_validate_erase(sh, test_addr, test_len);
  if (ret != 0) {
    return ret;
  }

  pbl_shell_print(sh, "OK");

  return 0;
}

static int prv_cmd_erased_sectors(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_flash_geometry *geo = FLASH->geometry;
  const bool show_subsectors = (argc > 1) && (strcmp(argv[1], "1") == 0);

  for (uint32_t addr = FLASH->base; addr - FLASH->base < geo->size; addr += geo->sector_size) {
    bool erased = pbl_flash_is_erased(FLASH, addr, geo->sector_size);

    pbl_shell_print(sh, "SECTOR - 0x%-8" PRIX32 " :: %s", addr, erased ? "true" : "false");
    if (show_subsectors && !erased) {
      for (uint32_t sub = addr; sub < addr + geo->sector_size; sub += geo->subsector_size) {
        pbl_shell_print(sh, "  SUBSECTOR - 0x%-8" PRIX32 " :: %s", sub,
                        pbl_flash_is_erased(FLASH, sub, geo->subsector_size) ? "true" : "false");
      }
    }
    pbl_task_wdt_feed_self();
  }

  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_flash, read, nullptr, "Write raw flash bytes to the console <addr> <len>",
                     prv_cmd_read, 3, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, fill, nullptr,
                     "Fill with an incrementing pattern <addr> <len> <start>", prv_cmd_fill, 4, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, validate, nullptr, "Erase, write and read back a test sector",
                     prv_cmd_validate, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, erased_sectors, nullptr,
                     "List erased sectors [1 = show subsectors]", prv_cmd_erased_sectors, 1, 1);
#else
#define STRESS_MAX_CHUNK  1024U
#define STRESS_READBACKS  8U
#define BENCH_MIN_TICKS   300U
#define BENCH_START_ITERS 4096U

static uint32_t s_stress_addr = FLASH_REGION_FIRMWARE_DEST_BEGIN;
static uint32_t s_stress_erased_end = FLASH_REGION_FIRMWARE_DEST_BEGIN;

static uint32_t prv_xorshift32(uint32_t x) {
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;

  return x;
}

static void prv_stress_cb(void *data) {
  const uint32_t sector_size = FLASH->geometry->sector_size;
  uint32_t iters = (uint32_t)(uintptr_t)data;
  uint32_t len = pbl_rand32() % STRESS_MAX_CHUNK;
  uint32_t seed = pbl_rand32() | 1U;
  uint32_t addr;
  uint32_t x;
  uint8_t *buf;

  if (iters == 0U) {
    PBL_LOG_ALWAYS("flash stress test complete");
    return;
  }

  buf = kernel_malloc(len);
  if (buf == nullptr) {
    PBL_LOG_ALWAYS("flash stress test: malloc of size %" PRIu32 " failed", len);
    system_task_add_callback(prv_stress_cb, (void *)(uintptr_t)(iters - 1U));
    return;
  }

  if (s_stress_addr + len > FLASH_REGION_FIRMWARE_DEST_END) {
    s_stress_addr = FLASH_REGION_FIRMWARE_DEST_BEGIN;
    s_stress_erased_end = FLASH_REGION_FIRMWARE_DEST_BEGIN;
  }

  addr = s_stress_addr;
  s_stress_addr += len;

  // Chunks are smaller than a sector
  if (s_stress_addr > s_stress_erased_end) {
    PBL_LOG_ALWAYS("flash stress test: erasing 0x%" PRIx32, s_stress_erased_end);
    if (pbl_flash_erase(FLASH, s_stress_erased_end, sector_size) != 0 ||
        !pbl_flash_is_erased(FLASH, s_stress_erased_end, sector_size)) {
      PBL_LOG_ALWAYS("flash stress test: erase of 0x%" PRIx32 " failed", s_stress_erased_end);
      kernel_free(buf);
      return;
    }
    s_stress_erased_end += sector_size;
  }

  x = seed;
  for (uint32_t i = 0U; i < len; i++) {
    buf[i] = (uint8_t)x;
    x = prv_xorshift32(x);
  }

  (void)pbl_flash_write(FLASH, addr, buf, len);

  for (uint32_t j = 0U; j < STRESS_READBACKS; j++) {
    memset(buf, 0, len);
    (void)pbl_flash_read(FLASH, addr, buf, len);

    x = seed;
    for (uint32_t i = 0U; i < len; i++) {
      if (buf[i] != (uint8_t)x) {
        PBL_LOG_ALWAYS("flash stress test: miscompare at 0x%" PRIx32 ", giving up", addr + i);
        kernel_free(buf);
        return;
      }
      x = prv_xorshift32(x);
    }
  }

  kernel_free(buf);

  PBL_LOG_ALWAYS("flash stress test: %" PRIu32 " bytes at 0x%" PRIx32 " OK, %" PRIu32 " to go", len,
                 addr, iters - 1U);
  system_task_add_callback(prv_stress_cb, (void *)(uintptr_t)(iters - 1U));
}

static int prv_cmd_stress(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t count;
  int ret;

  ret = prv_parse_u32(sh, argv[1], &count);
  if (ret != 0) {
    return ret;
  }

  // This wears the flash out: erases repeat on the same sectors far faster than specified
  pbl_shell_print(sh, "flash stress test running in background");
  system_task_add_callback(prv_stress_cb, (void *)(uintptr_t)count);

  return 0;
}

static int prv_benchmark(const struct pbl_shell *sh, size_t size) {
  const uint32_t window = 2U * FLASH->geometry->subsector_size;
  RtcTicks elapsed;
  uint32_t iters = BENCH_START_ITERS / 2U;
  void *buf;

  buf = kernel_malloc(size);
  if (buf == nullptr) {
    pbl_shell_error(sh, "OOM allocating read buffer");
    return -ENOMEM;
  }

  do {
    RtcTicks start = rtc_get_ticks();
    uint32_t offset = 0U;

    iters *= 2U;
    for (uint32_t i = 0U; i < iters; i++) {
      (void)pbl_flash_read(FLASH, FLASH_REGION_FIRMWARE_DEST_BEGIN + offset, buf, size);
      offset = (offset + size) % window;
    }

    elapsed = rtc_get_ticks() - start;
  } while (elapsed < BENCH_MIN_TICKS);

  pbl_shell_print(sh, "%u bytes: %" PRIu32 " iters in %" PRIu64 " ticks = %" PRIu32 " us/iter",
                  (unsigned int)size, iters, elapsed,
                  (uint32_t)((elapsed * 1000000U) / ((uint64_t)iters * RTC_TICKS_HZ)));

  kernel_free(buf);
  pbl_task_wdt_feed_self();

  return 0;
}

static int prv_cmd_benchmark(const struct pbl_shell *sh, size_t argc, char **argv) {
  static const size_t sizes[] = {4, 5, 16, 64, 128, 256, 512, 1024};

  for (size_t i = 0U; i < ARRAY_LENGTH(sizes); i++) {
    int ret = prv_benchmark(sh, sizes[i]);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_flash, stress, nullptr, "Write/read stress test in the background <count>",
                     prv_cmd_stress, 2, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, benchmark, nullptr, "Benchmark flash reads", prv_cmd_benchmark, 0,
                     0);
#endif

#ifdef CONFIG_FLASH_OTP
#define OTP_LOCK_CONFIRM "l0ckm3f0r3v3r"

static int prv_parse_region(const struct pbl_shell *sh, const char *str, uint8_t *region) {
  uint32_t val;
  int ret;

  ret = prv_parse_u32(sh, str, &val);
  if (ret != 0) {
    return ret;
  }

  if (val > UINT8_MAX) {
    pbl_shell_error(sh, "invalid region '%s'", str);
    return -EINVAL;
  }

  *region = (uint8_t)val;

  return 0;
}

static int prv_cmd_otp_info(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_flash_otp_info info;
  int ret;

  ret = pbl_flash_otp_get_info(FLASH, &info);
  if (ret != 0) {
    pbl_shell_error(sh, "no OTP (%d)", ret);
    return ret;
  }

  for (uint8_t i = 0U; i < info.n_regions; i++) {
    bool locked;

    ret = pbl_flash_otp_is_locked(FLASH, i, &locked);
    if (ret != 0) {
      pbl_shell_error(sh, "lock check failed (%d)", ret);
      return ret;
    }

    pbl_shell_print(sh, "%u: 0x%04" PRIx32 ", %" PRIu32 " bytes%s", i, i * info.region_len,
                    info.region_len, locked ? ", locked" : "");
  }

  if (FLASH->otp_layout != nullptr) {
    for (uint8_t i = 0U; i < FLASH->otp_layout->num_cells; i++) {
      const struct pbl_flash_otp_cell *cell = &FLASH->otp_layout->cells[i];

      pbl_shell_print(sh, "%s: 0x%04" PRIx32 ", %" PRIu32 " bytes", cell->name, cell->offset,
                      cell->len);
    }
  }

  return 0;
}

static int prv_cmd_otp_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint8_t buf[64];
  uint32_t offs;
  uint32_t len;
  int ret;

  ret = prv_parse_range(sh, argv[1], argv[2], &offs, &len);
  if (ret != 0) {
    return ret;
  }

  while (len > 0U) {
    uint32_t chunk = MIN(len, sizeof(buf));

    ret = pbl_flash_otp_read(FLASH, offs, buf, chunk);
    if (ret != 0) {
      pbl_shell_error(sh, "read failed (%d)", ret);
      return ret;
    }

    pbl_shell_print(sh, "OTP data at offset 0x%" PRIx32, offs);
    pbl_shell_hexdump(sh, buf, chunk);

    offs += chunk;
    len -= chunk;
  }

  return 0;
}

static int prv_cmd_otp_write(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t offs;
  uint32_t val;
  uint8_t byte;
  int ret;

  ret = prv_parse_u32(sh, argv[1], &offs);
  if (ret != 0) {
    return ret;
  }

  ret = prv_parse_u32(sh, argv[2], &val);
  if (ret != 0) {
    return ret;
  }

  if (val > UINT8_MAX) {
    pbl_shell_error(sh, "invalid byte '%s'", argv[2]);
    return -EINVAL;
  }

  byte = (uint8_t)val;

  return pbl_flash_otp_write(FLASH, offs, &byte, sizeof(byte));
}

static int prv_cmd_otp_erase(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint8_t region;
  int ret;

  ret = prv_parse_region(sh, argv[1], &region);
  if (ret != 0) {
    return ret;
  }

  return pbl_flash_otp_erase(FLASH, region);
}

static int prv_cmd_otp_lock(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint8_t region;
  int ret;

  if (strcmp(argv[2], OTP_LOCK_CONFIRM) != 0) {
    pbl_shell_error(sh, "locking is irreversible, confirm with '" OTP_LOCK_CONFIRM "'");
    return -EPERM;
  }

  ret = prv_parse_region(sh, argv[1], &region);
  if (ret != 0) {
    return ret;
  }

  return pbl_flash_otp_lock(FLASH, region);
}

static const struct pbl_shell_cmd sub_flash_otp[] = {
  PBL_SHELL_CMD(info, nullptr, "Show the OTP regions and cells", prv_cmd_otp_info),
  PBL_SHELL_CMD_ARG(read, nullptr, "Hex dump <offset> <len> of the OTP area", prv_cmd_otp_read, 3,
                    0),
  PBL_SHELL_CMD_ARG(write, nullptr, "Program a byte <offset> <value> of the OTP area",
                    prv_cmd_otp_write, 3, 0),
  PBL_SHELL_CMD_ARG(erase, nullptr, "Erase an OTP <region>", prv_cmd_otp_erase, 2, 0),
  PBL_SHELL_CMD_ARG(lock, nullptr, "Lock an OTP <region> forever <confirmation>", prv_cmd_otp_lock,
                    3, 0),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_SUBCMD_ADD(sub_flash, otp, sub_flash_otp, "One-time programmable area", nullptr, 0, 0);
#endif
