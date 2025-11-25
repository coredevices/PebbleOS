/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */


#include "applib/graphics/gtypes.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/gdraw_command_image.h"
#include "applib/graphics/gdraw_command_sequence.h"
#include "applib/graphics/gdraw_command_private.h"
#include "applib/graphics/graphics_private_raw.h"

#include "stubs_applib_resource.h"
#include "stubs_app_state.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_pbl_malloc.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"


#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#include "util_pbi.h"

static char *s_pbi2png_path;

// Stubs
void passert_failed(const char* filename, int line_number, const char* message, ...) {
  exit(EXIT_FAILURE);
}

void passert_failed_no_message(const char* filename, int line_number) {
  exit(EXIT_FAILURE);
}

void wtf(void) {
  exit(EXIT_FAILURE);
}

bool process_manager_compiled_with_legacy2_sdk(void) {
  return false;
}

const uint8_t *resource_get_builtin_bytes(ResAppNum app_num, uint32_t resource_id,
                                          uint32_t *num_bytes_out) {
  return NULL;
}

// Get the path of the filename before the extension
size_t prv_get_base_path(const char *filename, char *base) {
  char *ext = strrchr(filename, '.');

  size_t len;
  if (ext == NULL) {
    len = strlen(filename);
  } else {
    len = ext - filename;
  }
  strncpy(base, filename, len);
  base[len] = '\0';
  return len;
}

// Reset the graphics context, fill the background color and center the drawing box according to the
// bounds provided
static void prv_setup_context(GContext *ctx, GSize bounds) {
  // just a fake FB so that we can successfully call _context_init()
  // the actual pixel data we will draw into exists independently and will be allocated below
  FrameBuffer fb;
  framebuffer_init(&fb, &bounds);
  framebuffer_clear(&fb);
  // Reset graphics context
  graphics_context_init(ctx, &fb, GContextInitializationMode_System);

  const uint16_t row_size_bytes = bounds.w;
  ctx->dest_bitmap = (GBitmap){
    .addr = malloc(row_size_bytes * bounds.h),
    .bounds = (GRect){.size = bounds},
    .row_size_bytes = row_size_bytes,
    .info.version = 1,
    .info.format = GBitmapFormat8Bit,
  };

  ctx->draw_state.clip_box = ctx->dest_bitmap.bounds;
  ctx->draw_state.drawing_box = ctx->draw_state.clip_box;

  // Always use anti-aliased
  graphics_context_set_antialiased(ctx, true);

  // Setup the draw state
  graphics_context_set_fill_color(ctx, GColorElectricBlue);
  graphics_fill_rect(ctx, &(GRect){.size = bounds});
}

static void prv_teardown_context(GContext *ctx) {
  free(ctx->dest_bitmap.addr);
  ctx->dest_bitmap.addr = NULL;
}

// Read PDC sequence file an write to a sequence of PNGs
void prv_convert_sequence(const char *filename, void *data, size_t size) {
  GDrawCommandSequence *sequence = data;

  // Validate
  if (!gdraw_command_sequence_validate(sequence, size)) {
    printf("Invalid PDC sequence: %s\n", filename);
    return;
  }

  // Create directory from base name (with extension stripped)
  char output[strlen(filename) + 1 + strlen("_65535.png")]; // max size of a file path
  size_t len = prv_get_base_path(filename, output);

  // Write out each frame as a PNG
  for (int i = 0; i < gdraw_command_sequence_get_num_frames(sequence); i++) {
    // Set up the context for every frame
    GContext ctx;
    prv_setup_context(&ctx, gdraw_command_sequence_get_bounds_size(sequence));

    GDrawCommandFrame *frame = gdraw_command_sequence_get_frame_by_index(sequence, i);
    gdraw_command_frame_draw(&ctx, sequence, frame, GPoint(0, 0));

    // Write frames into new directory as a numbered sequence
    sprintf(&output[len], "_%d.png", i + 1);
    write_gbitmap_to_pbi(&ctx.dest_bitmap, output, s_pbi2png_path);
    prv_teardown_context(&ctx);
  }
}

// Read PDC image file and convert to PNG
void prv_convert_image(const char *filename, void *data, size_t size) {
  GDrawCommandImage *image = data;

  // Validate
  if (!gdraw_command_image_validate(image, size)) {
    printf("Invalid PDC image: %s\n", filename);
    return;
  }

  GContext ctx;
  prv_setup_context(&ctx, gdraw_command_image_get_bounds_size(image));

  gdraw_command_image_draw(&ctx, image, GPoint(0, 0));

  // Write output to file of the same name with extension replaced with .png
  char output[strlen(filename) + 5];  // add space for .pdc + '\0'
  prv_get_base_path(filename, output);
  strcat(output, ".png");

  write_gbitmap_to_pbi(&ctx.dest_bitmap, output, s_pbi2png_path);

  prv_teardown_context(&ctx);
}

static void prv_convert_pdc(const char *filename) {

  FILE *f = fopen(filename, "rb");
  if (f == NULL) {
    printf("File not found: %s\n", filename);
    return;
  }

  // Check that file is a PDC file
  char magic[4];
  if (fread(magic, sizeof(magic), 1, f) != 1) {
    printf("Failed to read PDC magic word: %s\n", filename);
    fclose(f);
    return;
  }

  // Read size of data
  size_t size;
  if (fread(&size, sizeof(size), 1, f) != 1) {
    printf("Failed to read PDC size: %s\n", filename);
    fclose(f);
    return;
  }

  // Read data into memory
  void *data = malloc(size);
  if (fread(data, size, 1, f) != 1) {
    printf("Failed to read PDC content: %s\n", filename);
    free(data);
    fclose(f);
    return;
  }
  fclose(f);

  if (strncmp(magic, "PDCI", sizeof(magic)) == 0) {
    prv_convert_image(filename, data, size);
  } else if (strncmp(magic, "PDCS", sizeof(magic)) == 0) {
    prv_convert_sequence(filename, data, size);
  } else {
    printf("Invalid file: %s\n", filename);
  }
  free(data);
}

int main(int argc, const char* argv[]) {
  // pdc2png file path always passed in as first argument
  if (argc == 1) {
    printf("No files specified. Pass a list of file paths to convert "
        "(e.g. pdc2png [path-to-file1] [path-to-file2] ...)\n");
  }

  char *dir = dirname((char *) argv[0]);
  printf("%s\n", dir);
  s_pbi2png_path = malloc(strlen(dir) + 1 + strlen(PBI2PNG_EXE) + 1);
  sprintf(s_pbi2png_path, "%s/%s", dir, PBI2PNG_EXE);

  // skip first argument (this file path)
  for (int i = 1; i < argc; i++) {
    // Treat all arguments as filenames and attempt to convert them as PDC into PNG
    // This seems to handle invalid arguments ok (it just can't find the files to open, so it fails
    // with a message saying so)
    printf("Converting %s...\n", argv[i]);
    prv_convert_pdc(argv[i]);
  }
}
