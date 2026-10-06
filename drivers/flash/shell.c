/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include <pbl/drivers/flash.h>
#include <pbl/drivers/rtc.h>
#include <pbl/logging/logging.h>
#include <pbl/shell/shell.h>
#include <pbl/task_wdt/task_wdt.h>

#include "flash_region/flash_region.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/system_task.h"
#include "pbl/util/math.h"
#include "system/passert.h"
#include "pbl/util/rand32.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#ifdef TEST_FLASH_LOCK_PROTECTION
#include <pbl/drivers/watchdog.h>

#include <cmsis_core.h>
#endif

PBL_SHELL_SUBCMD_SET_CREATE(sub_flash);
PBL_SHELL_CMD_REGISTER(flash, sub_flash, "Flash", NULL);

static int prv_parse_range(const struct pbl_shell *sh, const char *addr_str, const char *len_str,
                           uint32_t *addr, uint32_t *len) {
  unsigned long val;

  if (pbl_shell_strtoul(addr_str, &val) != 0 || val > UINT32_MAX) {
    pbl_shell_error(sh, "invalid address '%s'", addr_str);
    return -EINVAL;
  }
  *addr = val;

  if (pbl_shell_strtoul(len_str, &val) != 0 || val == 0 || val > UINT32_MAX) {
    pbl_shell_error(sh, "invalid length '%s'", len_str);
    return -EINVAL;
  }
  *len = val;

  return 0;
}

//! Some flash chips have an accelerated method of checking for erased sectors. This is a sanity
//! check against that method: it reads the bytes back and makes sure they are really erased.
static bool prv_is_really_erased(const struct pbl_shell *sh, uint32_t addr, bool is_subsector) {
  bool erased = is_subsector ? flash_subsector_is_erased(addr) : flash_sector_is_erased(addr);
  if (!erased) {
    return false;
  }

  uint8_t buffer[64];
  uint32_t end_addr = addr + (is_subsector ? SUBSECTOR_SIZE_BYTES : SECTOR_SIZE_BYTES);
  for (uint32_t i_addr = addr; i_addr < end_addr; i_addr += sizeof(buffer)) {
    flash_read_bytes(buffer, i_addr, sizeof(buffer));
    for (uint32_t j = 0; j < sizeof(buffer); j++) {
      if (buffer[j] != 0xFF) {
        if (sh != NULL) {
          pbl_shell_print(sh, "(sub)sector at 0x%" PRIX32 " not really erased, is_subsector: %d",
                          addr, is_subsector);
        } else {
          PBL_LOG_ALWAYS("(sub)sector at 0x%" PRIX32 " not really erased, is_subsector: %d", addr,
                         is_subsector);
        }
        return false;
      }
    }
  }

  return true;
}

static int prv_cmd_erase(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t address;
  uint32_t length;

  if (prv_parse_range(sh, argv[1], argv[2], &address, &length) != 0) {
    return -EINVAL;
  }

  pbl_shell_print(sh, "erasing sectors from 0x%" PRIx32 " for %" PRIu32 "b", address, length);

  const uint32_t end_address = address + length;
  const uint32_t aligned_end_address =
      (end_address + (SUBSECTOR_SIZE_BYTES - 1)) & SUBSECTOR_ADDR_MASK;

  flash_region_erase_optimal_range_no_watchdog(address, address, end_address, aligned_end_address);

  return 0;
}

static int prv_cmd_dump(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t address;
  uint32_t length;
  uint8_t buffer[128];

  if (prv_parse_range(sh, argv[1], argv[2], &address, &length) != 0) {
    return -EINVAL;
  }

  while (length > 0) {
    uint32_t chunk_size = MIN(length, sizeof(buffer));
    flash_read_bytes(buffer, address, chunk_size);

    pbl_shell_print(sh, "data at address 0x%" PRIx32, address);
    pbl_shell_hexdump(sh, buffer, chunk_size);

    address += chunk_size;
    length -= chunk_size;
    system_task_watchdog_feed();
  }

  return 0;
}

static int prv_cmd_crc(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t address;
  uint32_t length;

  if (prv_parse_range(sh, argv[1], argv[2], &address, &length) != 0) {
    return -EINVAL;
  }

  pbl_shell_print(sh, "CRC: %" PRIx32, flash_crc32_legacy(address, length));
  return 0;
}

#ifdef CONFIG_RECOVERY_FW
#define MAX_READ_FLASH_SIZE   1024
#define WRITE_PAGE_SIZE_BYTES 64

static int prv_cmd_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t address;
  uint32_t length;

  if (prv_parse_range(sh, argv[1], argv[2], &address, &length) != 0) {
    return -EINVAL;
  }

  uint8_t *buffer = kernel_malloc(MIN(MAX_READ_FLASH_SIZE, length));
  if (buffer == NULL) {
    pbl_shell_error(sh, "unable to allocate read buffer");
    return -ENOMEM;
  }

  while (length > 0) {
    uint32_t read_length = MIN(length, MAX_READ_FLASH_SIZE);

    flash_read_bytes(buffer, address, read_length);

    // raw bytes, not text
    for (uint32_t i = 0; i < read_length; i++) {
      pbl_shell_fprintf(sh, "%c", buffer[i]);
    }

    address += read_length;
    length -= read_length;
  }

  kernel_free(buffer);
  return 0;
}

static int prv_cmd_mode(const struct pbl_shell *sh, size_t argc, char **argv) {
  long mode;

  if (pbl_shell_strtol(argv[1], &mode) != 0) {
    pbl_shell_error(sh, "invalid mode '%s'", argv[1]);
    return -EINVAL;
  }

  flash_switch_mode(mode);
  return 0;
}

static int prv_cmd_fill(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t address;
  uint32_t length;
  unsigned long value;

  if (prv_parse_range(sh, argv[1], argv[2], &address, &length) != 0) {
    return -EINVAL;
  }

  if (pbl_shell_strtoul(argv[3], &value) != 0 || value > 0xFF) {
    pbl_shell_error(sh, "invalid value '%s'", argv[3]);
    return -EINVAL;
  }

  uint8_t page[WRITE_PAGE_SIZE_BYTES];
  for (uint32_t i = 0; i < WRITE_PAGE_SIZE_BYTES; i++) {
    page[i] = (uint8_t)(value++ & 0xFF);
  }

  uint32_t bytes_remaining = length;
  while (bytes_remaining > 0) {
    uint32_t bytes_to_write = MIN(bytes_remaining, WRITE_PAGE_SIZE_BYTES);

    flash_write_bytes(page, address, bytes_to_write);
    bytes_remaining -= bytes_to_write;
    address += bytes_to_write;
  }

  return 0;
}

static int prv_cmd_validate(const struct pbl_shell *sh, size_t argc, char **argv) {
  // just test one sector, which is probably less than the size of the region
  const uint32_t TEST_ADDR = FLASH_REGION_FIRMWARE_DEST_BEGIN;
  const uint32_t TEST_LENGTH = SECTOR_SIZE_BYTES;
  PBL_ASSERTN((TEST_ADDR & SECTOR_ADDR_MASK) == TEST_ADDR);
  PBL_ASSERTN((TEST_ADDR + TEST_LENGTH) <= FLASH_REGION_FIRMWARE_DEST_END);

  flash_erase_sector_blocking(TEST_ADDR);
  if (!flash_sector_is_erased(TEST_ADDR)) {
    pbl_shell_error(sh, "sector not erased");
    return -EIO;
  }

  const uint32_t BUFFER_SIZE = 256;
  uint8_t buffer[BUFFER_SIZE];
  for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
    buffer[i] = i;
  }
  for (uint32_t offset = 0; offset < TEST_LENGTH; offset += BUFFER_SIZE) {
    flash_write_bytes(buffer, TEST_ADDR + offset, BUFFER_SIZE);
  }

  for (uint32_t offset = 0; offset < TEST_LENGTH; offset += BUFFER_SIZE) {
    memset(buffer, 0, BUFFER_SIZE);
    const uint32_t addr = TEST_ADDR + offset;
    flash_read_bytes(buffer, addr, BUFFER_SIZE);
    for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
      if (buffer[i] != i) {
        pbl_shell_error(sh, "incorrect value at 0x%" PRIx32, addr + i);
        return -EIO;
      }
    }
  }

  // Stitching different types of flash ops together (single byte reads followed by memmaps) has
  // caused issues before.
  const uint32_t SHORT_TEST_LENGTH = 1000;
  for (uint32_t offset = 0; offset < SHORT_TEST_LENGTH; offset++) {
    uint8_t memmap_buffer[130]; // > 128 bytes, triggers a memmap read for QSPI
    memset(memmap_buffer, 0x00, sizeof(memmap_buffer));

    const uint32_t pre_addr = TEST_ADDR + offset - MIN(offset, 1);
    uint8_t pre_byte;
    flash_read_bytes(&pre_byte, pre_addr, sizeof(pre_byte));

    const uint32_t addr = TEST_ADDR + offset;
    size_t read_size = MIN(sizeof(memmap_buffer), SHORT_TEST_LENGTH - offset);
    flash_read_bytes(&memmap_buffer[0], addr, read_size);
    for (size_t i = 0; i < read_size; i++) {
      uint8_t want = (offset + i) & 0xff;
      if (memmap_buffer[i] != want) {
        pbl_shell_error(sh, "failed at offset %d, got: %d, wanted: %d", (int)offset,
                        (int)memmap_buffer[i], (int)want);
        break;
      }
    }
  }

  flash_erase_sector_blocking(TEST_ADDR);
  if (!flash_sector_is_erased(TEST_ADDR)) {
    pbl_shell_error(sh, "sector not erased");
    return -EIO;
  }

  pbl_shell_print(sh, "OK");
  return 0;
}

static int prv_cmd_erased_sectors(const struct pbl_shell *sh, size_t argc, char **argv) {
  const bool show_subsectors = (argc > 1) && (strcmp(argv[1], "1") == 0);

  for (uint32_t addr = 0; addr < BOARD_NOR_FLASH_SIZE; addr += SECTOR_SIZE_BYTES) {
    bool erased = prv_is_really_erased(sh, addr, false);
    pbl_shell_print(sh, "SECTOR - 0x%-6" PRIX32 " :: %s", addr, erased ? "true" : "false");
    if (show_subsectors && !erased) {
      for (uint32_t i = 0; i < (SECTOR_SIZE_BYTES / SUBSECTOR_SIZE_BYTES); i++) {
        const uint32_t sub_addr = (addr + (i * SUBSECTOR_SIZE_BYTES));
        bool sub_erased = prv_is_really_erased(sh, sub_addr, true);
        pbl_shell_print(sh, "  SUBSECTOR - 0x%-6" PRIX32 " :: %s", sub_addr,
                        sub_erased ? "true" : "false");
      }
    }
    pbl_task_wdt_feed_self();
  }

  return 0;
}

#ifdef CONFIG_OTP_FLASH
static int prv_parse_u32(const struct pbl_shell *sh, const char *str, uint32_t *out) {
  unsigned long val;

  if (pbl_shell_strtoul(str, &val) != 0 || val > UINT32_MAX) {
    pbl_shell_error(sh, "invalid value '%s'", str);
    return -EINVAL;
  }

  *out = val;
  return 0;
}

static int prv_cmd_sec_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t address;
  uint8_t val;

  if (prv_parse_u32(sh, argv[1], &address) != 0) {
    return -EINVAL;
  }

  if (flash_read_security_register(address, &val) != S_SUCCESS) {
    pbl_shell_error(sh, "unable to read security register");
    return -EIO;
  }

  pbl_shell_print(sh, "security register value: 0x%02x", val);
  return 0;
}

static int prv_cmd_sec_write(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t address;
  uint32_t value;

  if (prv_parse_u32(sh, argv[1], &address) != 0 || prv_parse_u32(sh, argv[2], &value) != 0) {
    return -EINVAL;
  }

  if (flash_write_security_register(address, (uint8_t)value) != S_SUCCESS) {
    pbl_shell_error(sh, "unable to write security register");
    return -EIO;
  }

  return 0;
}

static int prv_cmd_sec_erase(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t address;

  if (prv_parse_u32(sh, argv[1], &address) != 0) {
    return -EINVAL;
  }

  if (flash_erase_security_register(address) != S_SUCCESS) {
    pbl_shell_error(sh, "unable to erase security register");
    return -EIO;
  }

  return 0;
}

static int prv_cmd_sec_wipe(const struct pbl_shell *sh, size_t argc, char **argv) {
  const FlashSecurityRegisters *info = flash_security_registers_info();

  for (uint8_t i = 0U; i < info->num_sec_regs; i++) {
    if (flash_erase_security_register(info->sec_regs[i]) != S_SUCCESS) {
      pbl_shell_error(sh, "unable to erase security register");
      return -EIO;
    }
  }

  return 0;
}

static int prv_cmd_sec_info(const struct pbl_shell *sh, size_t argc, char **argv) {
  const FlashSecurityRegisters *info = flash_security_registers_info();

  if (info->sec_regs == NULL) {
    pbl_shell_print(sh, "no security registers");
    return 0;
  }

  pbl_shell_print(sh, "number of security registers: %d", info->num_sec_regs);
  for (int i = 0; i < info->num_sec_regs; i++) {
    bool locked;

    if (flash_security_register_is_locked(info->sec_regs[i], &locked) != S_SUCCESS) {
      pbl_shell_error(sh, "unable to check security register lock status");
      return -EIO;
    }

    pbl_shell_print(sh, "security register %d: 0x%08" PRIx32 " (locked: %u)", i,
                    (uint32_t)info->sec_regs[i], locked);
  }

  return 0;
}

static int prv_cmd_sec_lock(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t address;

  if (strcmp(argv[2], "l0ckm3f0r3v3r") != 0) {
    pbl_shell_error(sh, "invalid password");
    return -EPERM;
  }

  if (prv_parse_u32(sh, argv[1], &address) != 0) {
    return -EINVAL;
  }

  if (flash_lock_security_register(address) != S_SUCCESS) {
    pbl_shell_error(sh, "unable to lock security register");
    return -EIO;
  }

  return 0;
}

static const struct pbl_shell_cmd sub_flash_sec[] = {
  PBL_SHELL_CMD_ARG(read, NULL, "Read a security register <addr>", prv_cmd_sec_read, 2, 0),
  PBL_SHELL_CMD_ARG(write, NULL, "Write a security register <addr> <value>", prv_cmd_sec_write, 3,
                    0),
  PBL_SHELL_CMD_ARG(erase, NULL, "Erase a security register <addr>", prv_cmd_sec_erase, 2, 0),
  PBL_SHELL_CMD(wipe, NULL, "Erase all security registers", prv_cmd_sec_wipe),
  PBL_SHELL_CMD(info, NULL, "Show the security registers", prv_cmd_sec_info),
  PBL_SHELL_CMD_ARG(lock, NULL, "Lock a security register forever <addr> <password>",
                    prv_cmd_sec_lock, 3, 0),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_SUBCMD_ADD(sub_flash, sec, sub_flash_sec, "Security registers", NULL, 0, 0);
#endif

PBL_SHELL_SUBCMD_ADD(sub_flash, read, NULL, "Write raw flash bytes to the console <addr> <len>",
                     prv_cmd_read, 3, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, mode, NULL, "Switch the flash mode <mode>", prv_cmd_mode, 2, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, fill, NULL,
                     "Fill with an incrementing pattern <addr> <len> <start>", prv_cmd_fill, 4, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, validate, NULL, "Erase, write and read back a test sector",
                     prv_cmd_validate, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, erased_sectors, NULL, "List erased sectors [1 = show subsectors]",
                     prv_cmd_erased_sectors, 1, 1);
#else
static uint32_t prv_xorshift32(uint32_t seed) {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed < 5;
  return seed;
}

static uint32_t s_flash_stress_addr = FLASH_REGION_FIRMWARE_DEST_BEGIN;
static uint32_t s_flash_stress_last_sector = FLASH_REGION_FIRMWARE_DEST_BEGIN + SECTOR_SIZE_BYTES;

static void prv_flash_stress_callback(void *data) {
  int iters = (int)(intptr_t)data;

  if (iters == 0) {
    PBL_LOG_ALWAYS("flash stress test complete");
    return;
  }

  int bufsz = pbl_rand32() % 1024;
  uint8_t *buf = kernel_malloc(bufsz);
  if (!buf) {
    PBL_LOG_ALWAYS("flash stress test: malloc of size %d failed", bufsz);
    system_task_add_callback(prv_flash_stress_callback, (void *)(intptr_t)(iters - 1));
    return;
  }

  uint32_t lfsr_seed = pbl_rand32();
  if (lfsr_seed == 0) {
    lfsr_seed = 1;
  }

  uint32_t flash_addr = s_flash_stress_addr;
  s_flash_stress_addr += bufsz;
  if (s_flash_stress_addr >= FLASH_REGION_FIRMWARE_DEST_END) {
    s_flash_stress_addr = flash_addr = FLASH_REGION_FIRMWARE_DEST_BEGIN;
    s_flash_stress_addr += bufsz;
  }

  int miscompare = 0;

  // the beginning is already erased, chunks are always smaller than a sector
  uint32_t sector_address = flash_get_sector_base_address(flash_addr + bufsz);
  if (sector_address != s_flash_stress_last_sector) {
    PBL_LOG_ALWAYS("flash stress test: erasing flash address %" PRIx32, sector_address);
    flash_erase_sector_blocking(sector_address);
    s_flash_stress_last_sector = sector_address;
    if (!prv_is_really_erased(NULL, sector_address, false)) {
      PBL_LOG_ALWAYS("flash stress test: flash address %" PRIx32 " erase failed!", sector_address);
      miscompare = -1;
      goto bailout;
    }
  }

  uint32_t lfsr_cur = lfsr_seed;
  for (int i = 0; i < bufsz; i++) {
    buf[i] = lfsr_cur & 0xFF;
    lfsr_cur = prv_xorshift32(lfsr_cur);
  }

  flash_write_bytes((const uint8_t *)buf, flash_addr, bufsz);

  for (int j = 0; j < 8; j++) {
    memset(buf, 0, bufsz);
    flash_read_bytes(buf, flash_addr, bufsz);

    lfsr_cur = lfsr_seed;

    for (int i = 0; i < bufsz; i++) {
      if (buf[i] != (lfsr_cur & 0xFF)) {
        PBL_LOG_ALWAYS("flash stress test: readback %d: miscompare at offset %d (%" PRIx32
                       "): expected 0x%02" PRIx32 ", found 0x%02x",
                       j, i, flash_addr + i, lfsr_cur & 0xFF, buf[i]);
        miscompare++;
      }
      lfsr_cur = prv_xorshift32(lfsr_cur);
    }
    if (miscompare) {
      break;
    }
  }

bailout:
  kernel_free(buf);

  if (miscompare) {
    PBL_LOG_ALWAYS("flash stress test: %d miscompares on %d byte chunk at address %" PRIx32
                   "!  giving up",
                   miscompare, bufsz, flash_addr);
  } else {
    PBL_LOG_ALWAYS("flash stress test: %d bytes at address %" PRIx32 " OK; %d to go", bufsz,
                   flash_addr, iters - 1);
    system_task_add_callback(prv_flash_stress_callback, (void *)(intptr_t)(iters - 1));
  }
}

static int prv_cmd_stress(const struct pbl_shell *sh, size_t argc, char **argv) {
  long count;

  if (pbl_shell_strtol(argv[1], &count) != 0 || count < 0) {
    pbl_shell_error(sh, "invalid count '%s'", argv[1]);
    return -EINVAL;
  }

  // WARNING: this can shorten the life of the flash chip, it violates the "wait 90 seconds between
  // erases of the same sector" spec.
  pbl_shell_print(sh, "flash stress test running in background");
  system_task_add_callback(prv_flash_stress_callback, (void *)count);
  return 0;
}

static int prv_flash_benchmark(const struct pbl_shell *sh, size_t sz) {
  uint32_t flash_addr = FLASH_REGION_FIRMWARE_DEST_BEGIN;

  void *buf = kernel_malloc(sz);
  if (!buf) {
    pbl_shell_error(sh, "OOM allocating read buffer");
    return -ENOMEM;
  }

  pbl_shell_print(sh, "benchmarking %d bytes...", (int)sz);

  RtcTicks ticks_elapsed;
  int iters = 2048;

  do {
    RtcTicks ticks_start = rtc_get_ticks();

    iters *= 2;
    for (int i = 0; i < iters; i++) {
      flash_read_bytes(buf, flash_addr, sz);
      flash_addr += sz;
      flash_addr &= ~3;                           /* keep us aligned */
      flash_addr &= ~(SUBSECTOR_SIZE_BYTES << 1); /* keep us from wrapping too far */
    }

    ticks_elapsed = rtc_get_ticks() - ticks_start;
  } while (ticks_elapsed < 300);

  uint32_t us_per_tick = ticks_elapsed * 1000000 / (iters * RTC_TICKS_HZ);
  pbl_shell_print(sh, "  -> %d bytes: %d iters in %lld ticks = %" PRIu32 " us/iter", (int)sz, iters,
                  ticks_elapsed, us_per_tick);

  kernel_free(buf);

  pbl_task_wdt_feed_self();
  return 0;
}

static int prv_cmd_benchmark(const struct pbl_shell *sh, size_t argc, char **argv) {
  static const size_t sizes[] = {4, 5, 16, 64, 128, 256, 512, 1024};

  pbl_shell_print(sh, "running flash read benchmark");

  for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
    int ret = prv_flash_benchmark(sh, sizes[i]);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_flash, stress, NULL, "Write/read stress test in the background <count>",
                     prv_cmd_stress, 2, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, benchmark, NULL, "Benchmark flash reads", prv_cmd_benchmark, 0, 0);
#endif

#ifdef TEST_FLASH_LOCK_PROTECTION
void flash_expect_program_failure(bool expect_failure);

// Write over every region of the flash: if PRF still boots afterwards, the protected regions held.
static int prv_cmd_lock_test(const struct pbl_shell *sh, size_t argc, char **argv) {
  static uint8_t buf[2048] = {0};

  __disable_irq();

  for (int i = 0; i < 2; i++) {
    for (uint32_t addr = 0; addr < BOARD_NOR_FLASH_SIZE; addr += sizeof(buf)) {
      if ((addr - FLASH_REGION_SAFE_FIRMWARE_BEGIN) <
          (FLASH_REGION_SAFE_FIRMWARE_END - FLASH_REGION_SAFE_FIRMWARE_BEGIN)) {
        flash_expect_program_failure(true);
      }

      if ((addr % SECTOR_SIZE_BYTES) == 0) {
        pbl_shell_print(sh, "validated: 0x%" PRIx32, addr);
        flash_erase_sector_blocking(addr);
        flash_erase_sector_blocking(addr); // exercise the already erased check
      }

      flash_write_bytes(&buf[0], addr, sizeof(buf));

      flash_expect_program_failure(false);
      watchdog_feed();
    }
  }

  pbl_task_wdt_feed_self();
  __enable_irq();
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_flash, lock_test, NULL, "Write over the whole flash to test the locks",
                     prv_cmd_lock_test, 0, 0);
#endif

PBL_SHELL_SUBCMD_ADD(sub_flash, erase, NULL, "Erase the sectors covering <addr> <len>",
                     prv_cmd_erase, 3, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, crc, NULL, "Legacy CRC of <addr> <len>", prv_cmd_crc, 3, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, dump, NULL, "Hex dump <addr> <len>", prv_cmd_dump, 3, 0);

#endif
