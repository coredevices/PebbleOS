/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <pbl/drivers/flash.h>
#include <pbl/kernel/malloc.h>
#include <pbl/shell/shell.h>
#ifdef CONFIG_TASK_WDT
#include <pbl/task_wdt/task_wdt.h>
#endif
#include <pbl/util/math.h>

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
  const struct pbl_flash_geometry *geo = pbl_flash_get_geometry(FLASH);

  pbl_shell_print(sh, "%s: %" PRIu32 " bytes%s", pbl_device_name(&FLASH->dev), geo->size,
                  pbl_device_is_ready(&FLASH->dev) ? "" : " (not ready)");
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
    uint32_t chunk = PBL_MIN(len, sizeof(buf));

    ret = pbl_flash_read(FLASH, addr, buf, chunk);
    if (ret != 0) {
      pbl_shell_error(sh, "read failed (%d)", ret);
      return ret;
    }

    pbl_shell_print(sh, "data at address 0x%" PRIx32, addr);
    pbl_shell_hexdump(sh, buf, chunk);

    addr += chunk;
    len -= chunk;
#ifdef CONFIG_TASK_WDT
    pbl_task_wdt_feed_self();
#endif
  }

  return 0;
}

#ifdef CONFIG_FLASH_CRC
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
#endif

static int prv_cmd_unprotect(const struct pbl_shell *sh, size_t argc, char **argv) {
  return pbl_flash_unprotect(FLASH);
}

PBL_SHELL_SUBCMD_ADD(sub_flash, info, nullptr, "Show the flash", prv_cmd_info, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, erase, nullptr, "Erase the subsectors covering <addr> <len>",
                     prv_cmd_erase, 3, 0);
#ifdef CONFIG_FLASH_CRC
PBL_SHELL_SUBCMD_ADD(sub_flash, crc, nullptr, "Legacy CRC of <addr> <len>", prv_cmd_crc, 3, 0);
#endif
PBL_SHELL_SUBCMD_ADD(sub_flash, dump, nullptr, "Hex dump <addr> <len>", prv_cmd_dump, 3, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, unprotect, nullptr, "Remove the flash write protection",
                     prv_cmd_unprotect, 0, 0);

#ifdef CONFIG_FLASH_SHELL_TEST
#define MAX_READ_SIZE   1024U
#define FILL_CHUNK_SIZE 64U

static int prv_cmd_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t addr;
  uint32_t len;
  uint8_t *buf;
  int ret;

  ret = prv_parse_range(sh, argv[1], argv[2], &addr, &len);
  if (ret != 0) {
    return ret;
  }

  buf = pbl_malloc(PBL_MIN(MAX_READ_SIZE, len));
  if (buf == nullptr) {
    pbl_shell_error(sh, "unable to allocate read buffer");
    return -ENOMEM;
  }

  while (len > 0U) {
    uint32_t chunk = PBL_MIN(len, MAX_READ_SIZE);

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

  pbl_free(buf);

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
    uint32_t chunk = PBL_MIN(len, FILL_CHUNK_SIZE);

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

static int prv_cmd_erased_sectors(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_flash_geometry *geo = pbl_flash_get_geometry(FLASH);
  const bool show_subsectors = (argc > 1) && (strcmp(argv[1], "1") == 0);

  for (uint32_t addr = 0U; addr < geo->size; addr += geo->sector_size) {
    bool erased = pbl_flash_is_erased(FLASH, addr, geo->sector_size);

    pbl_shell_print(sh, "SECTOR - 0x%-8" PRIX32 " :: %s", addr, erased ? "true" : "false");
    if (show_subsectors && !erased) {
      for (uint32_t sub = addr; sub < addr + geo->sector_size; sub += geo->subsector_size) {
        pbl_shell_print(sh, "  SUBSECTOR - 0x%-8" PRIX32 " :: %s", sub,
                        pbl_flash_is_erased(FLASH, sub, geo->subsector_size) ? "true" : "false");
      }
    }
#ifdef CONFIG_TASK_WDT
    pbl_task_wdt_feed_self();
#endif
  }

  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_flash, read, nullptr, "Write raw flash bytes to the console <addr> <len>",
                     prv_cmd_read, 3, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, fill, nullptr,
                     "Fill with an incrementing pattern <addr> <len> <start>", prv_cmd_fill, 4, 0);
PBL_SHELL_SUBCMD_ADD(sub_flash, erased_sectors, nullptr,
                     "List erased sectors [1 = show subsectors]", prv_cmd_erased_sectors, 1, 1);
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
    uint32_t chunk = PBL_MIN(len, sizeof(buf));

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

  ret = pbl_flash_otp_erase(FLASH, region);
  if (ret != 0) {
    pbl_shell_error(sh, "erase failed (%d)", ret);
  }

  return ret;
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

  ret = pbl_flash_otp_lock(FLASH, region);
  if (ret != 0) {
    pbl_shell_error(sh, "lock failed (%d)", ret);
  }

  return ret;
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
