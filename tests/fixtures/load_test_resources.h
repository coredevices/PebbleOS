/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "flash_region/flash_region.h"
#include "resource/resource.h"
#include "resource/resource_version.auto.h"
#include "services/normal/filesystem/pfs.h"

#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "fake_spi_flash.h"

#define RESOURCES_FIXTURE_PATH "resources"
#define APP_RESOURCES_FIXTURE_NAME "app_resources.pbpack"
#define PUG_FIXTURE_NAME "pug.pbpack"
#define FRENCH_FIXTURE_NAME "fr_FR.pbpack"
#define CHINESE_FIXTURE_NAME "zh_CN.pbpack"

// We used to implicitly use the snowy pbpack for tintin and spalding unit tests; now it's explicit
#if PLATFORM_TINTIN || PLATFORM_SPALDING
#define SYSTEM_RESOURCES_FIXTURE_NAME "system_resources_snowy.pbpack"
#else
#define SYSTEM_RESOURCES_FIXTURE_NAME "system_resources_"PLATFORM_NAME".pbpack"
#endif

void load_resource_fixture_in_flash(const char *fixture_path, const char *name, bool is_next) {
  char res_path[strlen(CLAR_FIXTURE_PATH) + strlen(fixture_path) + strlen(name) + 3];
  sprintf(res_path, "%s/%s/%s", CLAR_FIXTURE_PATH, fixture_path, name);
  uint32_t address = is_next ? FLASH_REGION_SYSTEM_RESOURCES_BANK_1_BEGIN
                             : FLASH_REGION_SYSTEM_RESOURCES_BANK_0_BEGIN;
  fake_spi_flash_populate_from_file(res_path, address);
  resource_init_app(0, &SYSTEM_RESOURCE_VERSION);
}

void load_resource_fixture_on_pfs(const char *fixture_path, const char *name, const char *pfs_name) {
  char res_path[strlen(CLAR_FIXTURE_PATH) + strlen(fixture_path) + strlen(name) + 3];
  sprintf(res_path, "%s/%s/%s", CLAR_FIXTURE_PATH, fixture_path, name);
  // check that file exists and fits in buffer
  struct stat st;
  cl_assert(stat(res_path, &st) == 0);

  FILE *file = fopen(res_path, "r");
  cl_assert(file);

  uint8_t buf[st.st_size];
  // copy file to fake flash storage
  cl_assert(fread(buf, 1, st.st_size, file) > 0);

  int fd = pfs_open(pfs_name, OP_FLAG_WRITE, FILE_TYPE_STATIC, st.st_size);
  cl_assert(fd >= 0);
  int bytes_written = pfs_write(fd, buf, st.st_size);
  cl_assert(st.st_size == bytes_written);
  pfs_close(fd);
}

void load_system_resources_fixture(void) {
  fake_spi_flash_init(0 /* offset */, 0x1000000 /* length */);
  pfs_init(false /* run filesystem check */);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);
  resource_init();
}
