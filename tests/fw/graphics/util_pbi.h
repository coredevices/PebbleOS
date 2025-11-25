/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#define PATH_STRING_LENGTH 512

extern GBitmapDataRowInfo prv_gbitmap_get_data_row_info(const GBitmap *bitmap, uint16_t y);

bool write_gbitmap_to_pbi(GBitmap *bmp, const char *filepath, const char *pbi2png_path) {
  char pbi_path[PATH_STRING_LENGTH];
  char png_path[PATH_STRING_LENGTH];
  char *ext = NULL;

  strncpy(pbi_path, filepath, sizeof(pbi_path));
  ext = strrchr(pbi_path, '.');
  strncpy(ext + 1, "pbi", 3);

  strncpy(png_path, filepath, sizeof(png_path));
  ext = strrchr(png_path, '.');
  strncpy(ext + 1, "png", 3);

  FILE *file = fopen(pbi_path, "w");
  if (!file) {
    printf("Unable to open file: %s\n", pbi_path);
    return false;
  }
  // Just in case this output bitmap was created by hand.
  bmp->info.version = GBITMAP_VERSION_CURRENT;

  // PBL-24228 Support Circular PBIs
  uint16_t info_flags = bmp->info_flags;
#ifdef PLATFORM_SPALDING
  if(bmp->info.format == GBitmapFormat8BitCircular) {
    // Have to force output format to 8Bit;
    ((BitmapInfo*)&info_flags)->format = GBitmapFormat8Bit;
  }
#endif

  // use entire bounds to include entire image
  GRect entire_bounds = GRect(0, 0, 
                              bmp->bounds.origin.x + bmp->bounds.size.w, 
                              bmp->bounds.origin.y + bmp->bounds.size.h);

  fwrite(&bmp->row_size_bytes, sizeof(bmp->row_size_bytes), 1, file);
  fwrite(&info_flags, 2, 1, file);
  fwrite(&entire_bounds, sizeof(GRect), 1, file);

#ifdef PLATFORM_SPALDING
  if(bmp->info.format == GBitmapFormat8BitCircular) {
    for (int y = 0; y < entire_bounds.size.h; ++y) {
      // 8-Bit circular buffer is centered in padded rows, so just grab row and write DISP_COLS
      const GBitmapDataRowInfo dest_row_info = prv_gbitmap_get_data_row_info(bmp, y);
      uint8_t *bmp_row = dest_row_info.data;
      int x = 0;
      const uint8_t blank = 0;
      // PBL-24229 missing mask: data outside of circle is garbage from previous and next rows
      // Pad with blanks before min_x
      while (x < dest_row_info.min_x) {
        fwrite(&blank, 1, 1, file);
        x++;
      }
      if (x <= dest_row_info.max_x) {
        int length = dest_row_info.max_x - x + 1;
        fwrite(&bmp_row[dest_row_info.min_x], 1, length, file);
        x += length;
      }
      // Pad with blanks after max_x
      while (x < entire_bounds.size.w) {
        fwrite(&blank, 1, 1, file);
        x++;
      }
    }
  } else {
#endif
    size_t data_size = bmp->row_size_bytes * (entire_bounds.size.h);
    fwrite(bmp->addr, 1, data_size, file);
#ifdef PLATFORM_SPALDING
  }
#endif

  uint8_t palette_size = gbitmap_get_palette_size(gbitmap_get_format(bmp));
  if (palette_size > 0) {
    fwrite(bmp->palette, 1, palette_size * sizeof(*bmp->palette), file);
  }
  fclose(file);
  printf("PBI file written to: %s\n", pbi_path);

  int pid = fork();
  if (pid == 0) {
    char *args[] = {
      "python",
      (char *)pbi2png_path,
      (char *)pbi_path,
      (char *)png_path,
      NULL,
    };
    execvp("python", args);

    perror("execv"); // Error internal to execve
    exit(EXIT_FAILURE);
  }

  int status;
  if (wait(&status) >= 0 && !WIFEXITED(status)) {
    printf("FAILURE: pbi2png.py process exited with %d status. PNG file not written\n",
           WEXITSTATUS(status));
  } else {
    printf("PNG file written to: %s\n", png_path);
  }
  return true;
}

