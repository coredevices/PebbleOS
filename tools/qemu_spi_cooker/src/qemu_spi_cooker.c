/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "clar_asserts.h"

#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "services/normal/filesystem/pfs.h"
#include "util/math.h"

#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_print.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"
#include "fake_spi_flash.h"

void flash_region_erase_optimal_range_no_watchdog(
    uint32_t min_start, uint32_t max_start,
    uint32_t min_end, uint32_t max_end) {
}

static int prv_prebake_pfs(const char *filename) {
  struct stat st;
  if (stat(filename, &st) != 0) {
    return 1;
  }

  fake_spi_flash_init(0, st.st_size);
  fake_spi_flash_populate_from_file((char*)filename, 0);
  pfs_init(true);

  int fd = open(filename, O_RDWR);

  uint8_t *spi_image_file = mmap(NULL, st.st_size, PROT_WRITE, MAP_SHARED, fd, 0);
  flash_read_bytes(spi_image_file, 0, st.st_size);
  munmap(spi_image_file, st.st_size);

  close(fd);
  fake_spi_flash_cleanup();

  return 0;
}

int main(int argc, const char* argv[]) {
  // SPI image file path always passed in as first argument
  if (argc == 1) {
    printf("No file specified. Pass a path to a QEMU SPI image. "
        "(e.g. qemu_spi_cooker <spi_flash_img>)\n");
    return 1;
  }

  return prv_prebake_pfs(argv[1]);
}
