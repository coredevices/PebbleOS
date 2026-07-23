# Bitmap resources (PBI)

PBI is the raw bitmap format used for image resources and `GBitmap` data.
The authoritative description is the Doxygen block in
`src/fw/applib/graphics/gbitmap_pbi.h`; files are produced by
`tools/bitmapgen.py` (shipped in the app SDK) and loaded by
`gbitmap_init_with_data()` in `src/fw/applib/graphics/gbitmap.c`. The
`info_flags` bit layout is `BitmapInfo` in
`src/fw/applib/graphics/gtypes.h`.

A PBI file is a 12-byte header, then pixel data, then (for palettized
formats) the palette. All multi-byte fields are little-endian:

| Offset | Field            | Type       | Notes                                  |
| ------ | ---------------- | ---------- | -------------------------------------- |
| 0      | `row_size_bytes` | `uint16_t` | row stride                             |
| 2      | `info_flags`     | `uint16_t` | see below                              |
| 4      | origin           | 4 bytes    | legacy; ignored by the firmware        |
| 8      | `width`          | `uint16_t` |                                        |
| 10     | `height`         | `uint16_t` |                                        |
| 12     | pixel data       |            | `row_size_bytes × height` bytes        |
| …      | palette          |            | ARGB8 entries; palettized formats only |

`info_flags`:

- bit 0: heap-allocated flag; always 0 in files
- bits 1–3: pixel format (below)
- bit 4: palette-heap-allocated flag; always 0 in files
- bits 5–11: reserved
- bits 12–15: file version, currently 1 (version 0 files are 2.x-era
  1-bit bitmaps without a format field)

## Pixel formats

| Value | Format                     | Bits/pixel | Row stride                              |
| ----- | -------------------------- | ---------- | --------------------------------------- |
| 0     | `GBitmapFormat1Bit`        | 1          | word-aligned: `((width + 31) / 32) × 4` |
| 1     | `GBitmapFormat8Bit`        | 8          | `width`                                 |
| 2     | `GBitmapFormat1BitPalette` | 1          | byte-aligned: `ceil(width / 8)`         |
| 3     | `GBitmapFormat2BitPalette` | 2          | byte-aligned: `ceil(width / 4)`         |
| 4     | `GBitmapFormat4BitPalette` | 4          | byte-aligned: `ceil(width / 2)`         |

(`GBitmapFormat8BitCircular` exists in the firmware for the round
framebuffer but never appears in PBI files.)

- **1Bit**: 1 = white, 0 = black; pixels packed LSB-first within each
  32-bit word; padding bits are ignored.
- **8Bit**: one `GColor8` (ARGB8, 2 bits per channel) byte per pixel.
- **Palettized**: pixels are palette indices, packed MSB-first within
  each byte. The palette follows the pixel data and holds exactly
  2^bpp ARGB8 entries (2, 4 or 16); the SDK tooling reduces images to
  the smallest bit depth that covers the colors used.
