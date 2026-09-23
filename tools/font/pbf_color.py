# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Color glyph encoding for PBF fonts with FEATURE_COLOR.

Glyph body layout (follows the 5-byte glyph header), see
docs/reference/formats/font.md:

    uint8_t  encoding      bits 0-1: log2(bpp), bits 2-3: mode
    uint8_t  palette_size
    uint16_t data_len
    uint8_t  palette[palette_size]   GColor8 (ARGB2222)
    uint8_t  data[data_len]
"""

import struct

FEATURE_COLOR = 0x04

MODE_RAW = 0
MODE_RLE = 1
MODE_TINTED = 2

MAX_PALETTE_SIZE = 16
RLE_MAX_RUN = 16

BODY_HEADER_STRUCT = "<BBH"
BODY_HEADER_SIZE = struct.calcsize(BODY_HEADER_STRUCT)

_BPP_LOG2 = {1: 0, 2: 1, 4: 2, 8: 3}


def rgba_to_gcolor8(r, g, b, a):
    """Quantize RGBA8888 to GColor8. Fully transparent pixels become 0."""
    a2 = a // 85 if a < 255 else 3
    if a2 == 0:
        return 0
    return (a2 << 6) | ((r // 85) << 4) | ((g // 85) << 2) | (b // 85)


def gcolor8_to_rgba(c):
    a = (c >> 6) & 3
    if a == 0:
        return (0, 0, 0, 0)
    return (((c >> 4) & 3) * 85, ((c >> 2) & 3) * 85, (c & 3) * 85, a * 85)


def _pack_rows(values, width, height, bpp):
    out = bytearray()
    for y in range(height):
        acc = 0
        nbits = 0
        for x in range(width):
            acc = (acc << bpp) | values[y * width + x]
            nbits += bpp
            if nbits == 8:
                out.append(acc)
                acc = 0
                nbits = 0
        if nbits:
            out.append(acc << (8 - nbits))
    return bytes(out)


def _unpack_rows(data, width, height, bpp):
    row_bytes = (width * bpp + 7) // 8
    mask = (1 << bpp) - 1
    values = []
    for y in range(height):
        row = data[y * row_bytes : (y + 1) * row_bytes]
        for x in range(width):
            bit = x * bpp
            values.append((row[bit // 8] >> (8 - bpp - bit % 8)) & mask)
    return values


def _rle(indices):
    out = bytearray()
    i = 0
    while i < len(indices):
        run = 1
        while (
            i + run < len(indices)
            and indices[i + run] == indices[i]
            and run < RLE_MAX_RUN
        ):
            run += 1
        out.append(((run - 1) << 4) | indices[i])
        i += run
    return bytes(out)


def _body(encoding, palette, data):
    return (
        struct.pack(BODY_HEADER_STRUCT, encoding, len(palette), len(data))
        + bytes(palette)
        + data
    )


def encode_color(pixels, width, height):
    """Encode a list of GColor8 values (row-major) as the smallest body."""
    counts = {}
    for c in pixels:
        counts[c] = counts.get(c, 0) + 1
    palette = sorted(counts, key=lambda c: (-counts[c], c))

    if len(palette) > MAX_PALETTE_SIZE:
        return _body(_BPP_LOG2[8] | (MODE_RAW << 2), [], bytes(pixels))

    bpp = 1 if len(palette) <= 2 else 2 if len(palette) <= 4 else 4
    lut = {c: i for i, c in enumerate(palette)}
    indices = [lut[c] for c in pixels]

    raw = _pack_rows(indices, width, height, bpp)
    rle = _rle(indices)
    if len(rle) < len(raw):
        return _body(_BPP_LOG2[bpp] | (MODE_RLE << 2), palette, rle)
    return _body(_BPP_LOG2[bpp] | (MODE_RAW << 2), palette, raw)


def encode_tinted(bits, width, height):
    """Encode a 1-bit coverage glyph that is drawn in the text color."""
    return _body(
        _BPP_LOG2[1] | (MODE_TINTED << 2), [], _pack_rows(bits, width, height, 1)
    )


def decode(body, width, height):
    """Decode a color glyph body.

    Returns (tinted, values): values are 0/1 coverage bits when tinted, GColor8
    values otherwise.
    """
    encoding, palette_size, data_len = struct.unpack_from(BODY_HEADER_STRUCT, body)
    bpp = 1 << (encoding & 3)
    mode = (encoding >> 2) & 3
    palette = list(body[BODY_HEADER_SIZE : BODY_HEADER_SIZE + palette_size])
    data = body[BODY_HEADER_SIZE + palette_size :][:data_len]

    if mode == MODE_TINTED:
        return True, _unpack_rows(data, width, height, 1)
    if mode == MODE_RLE:
        indices = []
        for b in data:
            indices.extend([b & 0x0F] * ((b >> 4) + 1))
        indices = indices[: width * height]
    elif bpp == 8:
        return False, list(data[: width * height])
    else:
        indices = _unpack_rows(data, width, height, bpp)
    return False, [palette[i] for i in indices]
