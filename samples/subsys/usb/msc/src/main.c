/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <pbl/drivers/disk.h>
#include <pbl/drivers/disk/ram.h>
#include <pbl/usb/msc.h>
#include <pbl/usb/usbd.h>
#include <pbl/util/byteorder.h>

#define SECTOR_SIZE  512U
#define ROOT_ENTRIES 64U
#define ROOT_SECTORS (ROOT_ENTRIES * 32U / SECTOR_SIZE)
#define MAX_CLUSTERS 4084U
#define LABEL        "PEBBLEOS   "
#define README       "Hello from PebbleOS!\n"
/* 2026-01-01 */
#define FAT_DATE ((46U << 9) | (1U << 5) | 1U)

#ifdef CONFIG_SAMPLE_READ_ONLY
#define READ_ONLY true
#else
#define READ_ONLY false
#endif

PBL_USB_MSC_LUN_DEFINE(ram, &pbl_disk_ram, READ_ONLY);

static uint8_t s_sector[SECTOR_SIZE];

static int prv_write(uint32_t sector) {
  int ret = pbl_disk_write(&pbl_disk_ram, sector, 1U, s_sector);

  memset(s_sector, 0, sizeof(s_sector));

  return ret;
}

static void prv_dirent(uint8_t *e, const char *name, uint8_t attr, uint16_t cluster,
                       uint32_t size) {
  memcpy(e, name, 11U);
  e[11] = attr;
  pbl_put_le16(FAT_DATE, &e[24]);
  pbl_put_le16(cluster, &e[26]);
  pbl_put_le32(size, &e[28]);
}

/* A FAT12 file system with a label and README.TXT in its first cluster. */
static int prv_format(uint32_t sectors) {
  uint32_t spc = 1U;
  uint32_t fat;
  uint32_t data;
  int ret;

  while ((sectors - 1U - ROOT_SECTORS) / spc > MAX_CLUSTERS) {
    spc *= 2U;
  }
  fat = (((sectors - 1U - ROOT_SECTORS) / spc + 2U) * 3U / 2U + SECTOR_SIZE - 1U) / SECTOR_SIZE;
  data = 1U + 2U * fat + ROOT_SECTORS;

  memcpy(s_sector, "\xeb\x3c\x90PEBBLEOS", 11U);
  pbl_put_le16(SECTOR_SIZE, &s_sector[11]);
  s_sector[13] = (uint8_t)spc;
  pbl_put_le16(1U, &s_sector[14]);
  s_sector[16] = 2U;
  pbl_put_le16(ROOT_ENTRIES, &s_sector[17]);
  pbl_put_le16((uint16_t)sectors, &s_sector[19]);
  s_sector[21] = 0xf8U;
  pbl_put_le16((uint16_t)fat, &s_sector[22]);
  s_sector[38] = 0x29U;
  pbl_put_le32(0x50424c45U, &s_sector[39]);
  memcpy(&s_sector[43], LABEL "FAT12   ", 19U);
  s_sector[510] = 0x55U;
  s_sector[511] = 0xaaU;
  ret = prv_write(0U);
  if (ret != 0) {
    return ret;
  }

  /* Both FATs: the media byte, then cluster 2 as the last of README.TXT. */
  for (uint32_t i = 0U; i < 2U * fat; i++) {
    if (i % fat == 0U) {
      memcpy(s_sector, "\xf8\xff\xff\xff\x0f", 5U);
    }
    ret = prv_write(1U + i);
    if (ret != 0) {
      return ret;
    }
  }

  for (uint32_t i = 0U; i < ROOT_SECTORS; i++) {
    if (i == 0U) {
      prv_dirent(&s_sector[0], LABEL, 0x08U, 0U, 0U);
      prv_dirent(&s_sector[32], "README  TXT", 0x20U, 2U, sizeof(README) - 1U);
    }
    ret = prv_write(1U + 2U * fat + i);
    if (ret != 0) {
      return ret;
    }
  }

  for (uint32_t i = 0U; i < spc; i++) {
    if (i == 0U) {
      memcpy(s_sector, README, sizeof(README) - 1U);
    }
    ret = prv_write(data + i);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

int main(void) {
  struct pbl_disk_info info;
  int ret;

  ret = pbl_disk_get_info(&pbl_disk_ram, &info);
  if (ret == 0) {
    ret = (info.sector_size == SECTOR_SIZE && info.sector_count <= UINT16_MAX)
              ? prv_format(info.sector_count)
              : -ENOTSUP;
  }
  if (ret != 0) {
    fprintf(stderr, "msc: formatting the RAM disk failed (%d)\n", ret);
    return ret;
  }
  printf("msc: RAM disk of %u KiB, FAT12 with README.TXT%s\n",
         (unsigned int)(info.sector_count * SECTOR_SIZE / 1024U), READ_ONLY ? ", read-only" : "");

  ret = pbl_usbd_enable();
  if (ret != 0) {
    fprintf(stderr, "msc: USB failed (%d)\n", ret);
    return ret;
  }

  /* The device now serves the host from the controller's interrupt and the system work queue. */
  printf("msc: ready\n");

  return 0;
}
