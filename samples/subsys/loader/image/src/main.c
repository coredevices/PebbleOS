/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/loader/loader.h>

#define BSS_SIZE 16U

/*
 * An image as a linker would emit it: a header, pointer slots holding offsets into the image,
 * the data they point to, then the relocation table listing the slots. Only the file part is
 * copied; the zero-filled part after it is not stored.
 */
struct image {
  char magic[4];
  uint32_t version;
  uint32_t greeting;
  uint32_t farewell;
  uint32_t counter;
  char strings[24];
  uint32_t relocs[3];
};

#define FILE_SIZE offsetof(struct image, relocs)

static const struct image s_image = {
  .magic = {'I', 'M', 'G', '0'},
  .version = 1,
  .greeting = offsetof(struct image, strings),
  .farewell = offsetof(struct image, strings) + 6,
  .counter = FILE_SIZE,
  .strings = "Hello\0Goodbye",
  .relocs = {
    offsetof(struct image, greeting),
    offsetof(struct image, farewell),
    offsetof(struct image, counter),
  },
};

static const struct pbl_loader_image s_layout = {
  .file_size = FILE_SIZE,
  .mem_size = FILE_SIZE + BSS_SIZE,
  .header_size = offsetof(struct image, greeting),
  .reloc_offset = offsetof(struct image, relocs),
  .reloc_count = 3,
};

/* The source could be flash or a file; here it is a copy in the firmware. */
static int prv_read(void *ctx, size_t offset, void *buf, size_t len) {
  const uint8_t *src = ctx;

  if (offset > sizeof(s_image) || len > sizeof(s_image) - offset) {
    return -EINVAL;
  }

  memcpy(buf, &src[offset], len);

  return 0;
}

static uint32_t s_ram[32];

static int prv_load(const struct pbl_loader_image *layout, const void *image, void *dst,
                    size_t dst_size) {
  const struct pbl_loader_source src = {.read = prv_read, .ctx = (void *)image};
  int ret;

  ret = pbl_loader_copy(layout, &src, dst, dst_size);
  if (ret < 0) {
    return ret;
  }

  /* Each slot gets the address of what it points to, now that the image has its place. */
  return pbl_loader_relocate(layout, &src, dst, dst_size, nullptr, nullptr);
}

int main(void) {
  const struct image *img = (const struct image *)s_ram;
  struct pbl_loader_image big = s_layout;
  struct image bad = s_image;
  int ret;

  ret = prv_load(&s_layout, &s_image, s_ram, sizeof(s_ram));
  if (ret < 0) {
    fprintf(stderr, "loader: load failed (%d)\n", ret);
    return ret;
  }

  printf("loader: image %.4s version %u, %u bytes stored, %u in memory\n", img->magic,
         (unsigned int)img->version, (unsigned int)s_layout.file_size,
         (unsigned int)s_layout.mem_size);
  printf("loader: greeting \"%s\", farewell \"%s\"\n", (const char *)(uintptr_t)img->greeting,
         (const char *)(uintptr_t)img->farewell);

  uint32_t *counter = (uint32_t *)(uintptr_t)img->counter;

  printf("loader: counter in the zero-filled part: %s, starts at %u\n",
         (uint8_t *)counter == (uint8_t *)s_ram + FILE_SIZE ? "yes" : "no", (unsigned int)*counter);

  /* A relocation must not patch the header. */
  bad.relocs[0] = offsetof(struct image, version);
  printf("loader: relocation into the header: %s\n",
         prv_load(&s_layout, &bad, s_ram, sizeof(s_ram)) == -EINVAL ? "-EINVAL" : "loaded");

  big.mem_size = sizeof(s_ram) + 1U;
  printf("loader: image larger than its memory: %s\n",
         prv_load(&big, &s_image, s_ram, sizeof(s_ram)) == -ENOMEM ? "-ENOMEM" : "loaded");

  printf("Sample done\n");

  return 0;
}
