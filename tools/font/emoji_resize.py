#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Scott Anderson
# SPDX-License-Identifier: Apache-2.0

"""
Build a larger emoji font from the Noto Emoji glyphs of a smaller one.

Only the glyphs the smaller font rendered from Noto Emoji are rendered
again, at the source font's pixel size and baseline scaled to the new
size. Every other glyph, every glyph its color variant colors, and the
flag letters are left out, so the firmware draws them from the smaller
font.

Usage:
    python emoji_resize.py EMOJI_28.pbf EMOJI_28~color.pbf \
        --noto NotoEmoji.ttf --size 36 -o resources/normal/base/pbf
"""

import argparse
import contextlib
import copy
import io
import os
import sys
import tempfile
from collections import Counter

import freetype
import pbf_extract
import pbf_repack
from PIL import Image

# Flag letters always draw as the flag, which isn't Noto Emoji
REGIONAL_INDICATORS = range(0x1F1E6, 0x1F200)


class NotoRenderer:
    def __init__(self, ttf_path, gsub=False):
        with open(ttf_path, "rb") as f:
            data = bytearray(f.read())
        if not gsub:
            # FreeType built with HarfBuzz hints GSUB glyphs differently.
            # Rename the table so that all builds give the same result.
            for i in range(int.from_bytes(data[4:6], "big")):
                tag = slice(12 + 16 * i, 16 + 16 * i)
                if data[tag] == b"GSUB":
                    data[tag] = b"gsub"
        self.face = freetype.Face(io.BytesIO(bytes(data)))

    def render(self, codepoint, px):
        """Render a 1-bit glyph, or return None if the font lacks it."""
        if not self.face.get_char_index(codepoint):
            return None
        self.face.set_pixel_sizes(0, px)
        self.face.load_char(
            chr(codepoint), freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO
        )
        glyph = self.face.glyph
        bitmap = glyph.bitmap
        bits = [
            (bitmap.buffer[y * bitmap.pitch + x // 8] >> (7 - x % 8)) & 1
            for y in range(bitmap.rows)
            for x in range(bitmap.width)
        ]
        return {
            "width": bitmap.width,
            "height": bitmap.rows,
            "bits": bits,
            "left": glyph.bitmap_left,
            "top": glyph.bitmap_top,
            "advance": glyph.advance.x // 64,
        }


def glyph_bits(path, glyph):
    img = Image.open(path).convert("L")
    return [
        1 if img.getpixel((x, y)) < 128 else 0
        for y in range(glyph["height"])
        for x in range(glyph["width"])
    ]


def noto_match(noto, glyph, base_dir, px):
    """Return the baseline row if px reproduces the glyph exactly, else None."""
    if not glyph["width"]:
        return None
    r = noto.render(glyph["codepoint"], px)
    if not r or (r["width"], r["height"]) != (glyph["width"], glyph["height"]):
        return None
    if (glyph["left_offset"], glyph["advance"]) != (r["left"], r["advance"]):
        return None
    if r["bits"] != glyph_bits(os.path.join(base_dir, glyph["file"]), glyph):
        return None
    return glyph["top_offset"] + r["top"]


def detect_noto(noto, manifest, base_dir):
    """Find the pixel size and baseline row the source font rendered Noto at."""
    best = None
    glyphs = [g for g in manifest["glyphs"] if g["width"]]
    max_height = manifest["max_height"]
    for px in range(max_height // 2, max_height + 1):
        baselines = Counter(
            b for g in glyphs if (b := noto_match(noto, g, base_dir, px)) is not None
        )
        if baselines:
            baseline, count = baselines.most_common(1)[0]
            if not best or count > best[2]:
                best = (px, baseline, count)
    if not best:
        sys.exit("No glyph in the source font matches Noto Emoji")
    return best[:2]


def ink_rows(img, top):
    """Return the first and end ink rows of a 1-bit image in font rows, or None."""
    box = Image.eval(img.convert("L"), lambda v: 255 - v).getbbox()
    return (top + box[1], top + box[3]) if box else None


def center_shift(src, src_png, top, img, src_size, size):
    """Return the rows to move a glyph so that the space above and below its
    ink has the same ratio as in the source glyph."""
    want, got = ink_rows(Image.open(src_png), src["top_offset"]), ink_rows(img, top)
    if not want or not got:
        return 0
    above, below = max(0, want[0]), max(0, src_size - want[1])
    if not above + below:
        return 0
    free = size - (got[1] - got[0])
    return max(0, round(free * above / (above + below))) - got[0]


def build(args, work):
    mono_dir = os.path.join(work, "src")
    color_dir = os.path.join(work, "src_color")
    with contextlib.redirect_stdout(io.StringIO()):
        mono = pbf_extract.extract_pbf(args.mono, mono_dir)
        color = pbf_extract.extract_pbf(args.color, color_dir)
    colored = {g["codepoint"] for g in color["glyphs"] if g.get("color")}

    # The source font was rendered with GSUB, so match it with GSUB
    src_noto = NotoRenderer(args.noto, gsub=True)
    noto = NotoRenderer(args.noto)
    src_px, src_baseline = detect_noto(src_noto, mono, mono_dir)
    src_size = mono["max_height"]
    dst_px = round(src_px * args.size / src_size)
    dst_baseline = dst_px + round((src_baseline - src_px) * args.size / src_size)
    print(f"Noto Emoji at {src_px} px -> {dst_px} px, baseline row {dst_baseline}")

    out = copy.deepcopy(mono)
    out["glyphs"] = []
    os.makedirs(os.path.join(work, "out", "glyphs"))
    counts = Counter()

    for g in mono["glyphs"]:
        cp = g["codepoint"]
        if cp in REGIONAL_INDICATORS:
            counts["left out as flag letters"] += 1
            continue
        if noto_match(src_noto, g, mono_dir, src_px) != src_baseline:
            counts["left out as not Noto Emoji"] += 1
            continue
        if cp in colored:
            counts["left out as color"] += 1
            continue
        r = noto.render(cp, dst_px)
        left, top = r["left"], dst_baseline - r["top"]
        img = Image.new("1", (r["width"], r["height"]), 1)
        img.putdata([0 if b else 1 for b in r["bits"]])
        mono_png = os.path.join(mono_dir, g["file"])
        top += center_shift(g, mono_png, top, img, src_size, args.size)
        img.save(os.path.join(work, "out", g["file"]))
        out["glyphs"].append(
            dict(
                g,
                width=r["width"],
                height=r["height"],
                left_offset=left,
                top_offset=top,
                advance=r["advance"],
            )
        )
        counts["rendered from Noto Emoji"] += 1

    print(", ".join(f"{n} {what}" for what, n in counts.items()))

    name = f"EMOJI_{args.size}.pbf"
    out["max_height"] = args.size
    out["source"] = name
    out["_manifest_path"] = os.path.join(work, "out", "font.json")
    with contextlib.redirect_stdout(io.StringIO()):
        ok = pbf_repack.build_pbf(out, os.path.join(args.output, name))
    if not ok:
        sys.exit(f"Failed to pack {name}")
    print(f"Wrote {os.path.join(args.output, name)}")


def main():
    parser = argparse.ArgumentParser(
        description="Build a larger emoji font from a smaller one"
    )
    parser.add_argument("mono", help="Source 1-bit emoji font, e.g. EMOJI_28.pbf")
    parser.add_argument("color", help="Its color variant, e.g. EMOJI_28~color.pbf")
    parser.add_argument("--noto", required=True, help="Noto Emoji TTF file")
    parser.add_argument("--size", type=int, required=True, help="Target font size")
    parser.add_argument("-o", "--output", required=True, help="Output directory")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as work:
        build(args, work)
    return 0


if __name__ == "__main__":
    sys.exit(main())
