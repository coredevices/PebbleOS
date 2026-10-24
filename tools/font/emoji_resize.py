#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Scott Anderson
# SPDX-License-Identifier: Apache-2.0

"""
Build a larger emoji font from a smaller one and its color variant.

Glyphs Noto Emoji has are rendered from it at the larger size, at the pixel
size and baseline the source font used for the glyphs it rendered from Noto.
The geometric glyphs Noto Emoji lacks are drawn again at the larger size
from their polygons, so edges stay straight. Nothing is scaled up from a
bitmap; a glyph that is neither is an error.

With --noto-color, color glyphs are instead drawn from Noto Emoji at the
larger size and colored from the Noto Color Emoji art; see emoji_color.

Usage:
    python emoji_resize.py EMOJI_28.pbf EMOJI_28~color.pbf \
        --noto NotoEmoji.ttf --noto-color noto-emoji/svg --size 36 \
        -o resources/normal/base/pbf
"""

import argparse
import contextlib
import copy
import io
import multiprocessing
import os
import sys
import tempfile
from collections import Counter

import emoji_color
import emoji_color_overrides
import freetype
import pbf_extract
import pbf_repack
from PIL import Image, ImageDraw

# Glyphs drawn from another Noto Emoji glyph's lines
NOTO_ALIASES = {0x2661: 0x1F90D, 0x1F90E: 0x1F90D}

# Geometric glyphs Noto Emoji lacks, as polygons through the centers of
# their 28 point glyphs' pixels: ((width, height), [(points, fill)]). They
# are drawn at the new size instead of scaled, so edges stay straight.
SHAPES = {
    0x2192: (
        (13, 5),
        [
            # A stem a pixel tall, 3 pixels at 36 point
            ([(0, 1.5), (10, 1.5), (10, 2.5), (0, 2.5)], 1),
            ([(10, 0), (12, 2), (10, 4)], 1),
        ],
    ),
    0x25AF: (
        (8, 14),
        [
            ([(0, 0), (7, 0), (7, 13), (0, 13)], 1),
            ([(1, 1), (6, 1), (6, 12), (1, 12)], 0),
        ],
    ),
    # Drawn as wide as tall, so its sides step two pixels a row
    0x25BA: ((8, 9), [([(0, 0), (7, 4), (0, 8)], 1)], "square"),
    # A five pointed star between its points
    0x2605: ((15, 13), "star", [(7, 0), (14, 4), (12, 12), (2, 12), (0, 4)]),
}


class NotoRenderer:
    def __init__(self, ttf_path, gsub=False):
        with open(ttf_path, "rb") as f:
            data = bytearray(f.read())
        if not gsub:
            # FreeType built with HarfBuzz, like freetype-py's wheels, hints
            # glyphs reachable through GSUB differently; hiding the table
            # renders the same with every build
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


def star(tips):
    """The outline of a star through five tips, with the inner corners
    where the lines between them cross."""

    def cross(p1, p2, p3, p4):
        d = (p1[0] - p2[0]) * (p3[1] - p4[1]) - (p1[1] - p2[1]) * (p3[0] - p4[0])
        t = ((p1[0] - p3[0]) * (p3[1] - p4[1]) - (p1[1] - p3[1]) * (p3[0] - p4[0])) / d
        return p1[0] + t * (p2[0] - p1[0]), p1[1] + t * (p2[1] - p1[1])

    out = []
    for i in range(5):
        out.append(tips[i])
        out.append(
            cross(tips[i], tips[(i + 2) % 5], tips[(i + 1) % 5], tips[(i + 4) % 5])
        )
    return out


def shape_size(cp, exact_w, exact_h):
    """The new size of a SHAPES glyph: odd where the source is, so points
    and centers stay one pixel."""
    src = SHAPES[cp][0]
    size = tuple(
        max(1, 2 * round((e - 1) / 2) + 1) if n % 2 else round(e)
        for n, e in zip(src, (exact_w, exact_h))
    )
    return (size[1], size[1]) if SHAPES[cp][-1] == "square" else size


def draw_shape(cp, width, height):
    """A mask of a SHAPES glyph at a new size, with its pixel centers moved
    so the first and last rows and columns stay first and last."""
    (src_w, src_h), polys, *_ = SHAPES[cp]
    if polys == "star":
        polys = [(star(SHAPES[cp][2]), 1)]
    sx, sy = (width - 1) / (src_w - 1), (height - 1) / (src_h - 1)
    mask = Image.new("L", (width, height), 0)
    d = ImageDraw.Draw(mask)
    for points, fill in polys:
        pts = [(round(x * sx), round(y * sy)) for x, y in points]
        if len(pts) == 2:
            d.line(pts, fill=255 * fill)
        else:
            d.polygon(pts, fill=255 * fill, outline=255 * fill)
    return mask


def shape_image(cp, width, height, path, color):
    """Draw a SHAPES glyph in the color of its source image."""
    mask = draw_shape(cp, width, height)
    if not color:
        img = Image.new("1", (width, height), 1)
        img.paste(0, mask=mask)
        return img
    src = Image.open(path).convert("RGBA")
    ink = Counter(p for p in src.get_flattened_data() if p[3]).most_common(1)[0][0]
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    img.paste(ink, mask=mask)
    return img


def ink_rows(img, top):
    """The first and past-last rows a 1-bit image inks, on the line."""
    box = Image.eval(img.convert("L"), lambda v: 255 - v).getbbox()
    return (top + box[1], top + box[3]) if box else None


def center_shift(src, src_png, top, img, src_size, size):
    """Rows to move a glyph so the space above and below its ink splits as
    it does in the source glyph."""
    want, got = ink_rows(Image.open(src_png), src["top_offset"]), ink_rows(img, top)
    if not want or not got:
        return 0
    above, below = max(0, want[0]), max(0, src_size - want[1])
    if not above + below:
        return 0
    free = size - (got[1] - got[0])
    return max(0, round(free * above / (above + below))) - got[0]


def color_glyph(job):
    """Color one glyph from the Noto Color art, cropped to its pixels:
    (image, left, top) in the frame, or None."""
    ttf, cp, svg, ref_png = job
    ov = emoji_color_overrides.OV.get(cp, {})
    m = emoji_color.frame(NotoRenderer(ttf), NOTO_ALIASES.get(cp, cp))
    if m is None:
        return None
    art = emoji_color.align(m, emoji_color.render_svg(svg))
    ref = Image.open(ref_png).convert("RGBA")
    img = emoji_color.colorize(m, art, ref, ov)
    box = img.getbbox()
    return img.crop(box), box[0] - emoji_color.OX, box[1]


def color_glyphs(args, color, color_dir, dst_px, dst_baseline):
    """Colored glyphs for the color glyphs with Noto Color art, by codepoint."""
    if (dst_px, dst_baseline) != (emoji_color.PX, emoji_color.BASELINE):
        sys.exit(
            f"Coloring needs Noto Emoji at {emoji_color.PX} px on baseline row "
            f"{emoji_color.BASELINE}"
        )
    jobs = []
    for g in color["glyphs"]:
        svg = os.path.join(args.noto_color, f"emoji_u{g['codepoint']:04x}.svg")
        bw = emoji_color_overrides.OV.get(g["codepoint"], {}).get("bw")
        if g.get("color") and g["width"] and os.path.exists(svg) and not bw:
            jobs.append(
                (args.noto, g["codepoint"], svg, os.path.join(color_dir, g["file"]))
            )
    with multiprocessing.Pool() as pool:
        done = pool.map(color_glyph, jobs)
    return {job[1]: r for job, r in zip(jobs, done) if r}


def build(args, work):
    mono_dir = os.path.join(work, "src")
    color_dir = os.path.join(work, "src_color")
    with contextlib.redirect_stdout(io.StringIO()):
        mono = pbf_extract.extract_pbf(args.mono, mono_dir)
        color = pbf_extract.extract_pbf(args.color, color_dir)
    if [g["codepoint"] for g in mono["glyphs"]] != [
        g["codepoint"] for g in color["glyphs"]
    ]:
        sys.exit("The mono and color fonts have different glyphs")

    # The source font was rendered with GSUB, so match it with GSUB
    src_noto = NotoRenderer(args.noto, gsub=True)
    noto = NotoRenderer(args.noto)
    src_px, src_baseline = detect_noto(src_noto, mono, mono_dir)
    src_size = mono["max_height"]
    scale = args.size / src_size
    dst_px = round(src_px * scale)
    dst_baseline = dst_px + round((src_baseline - src_px) * scale)
    ratio = dst_px / src_px
    print(f"Noto Emoji at {src_px} px -> {dst_px} px, baseline row {dst_baseline}")

    colored = {}
    if args.noto_color:
        colored = color_glyphs(args, color, color_dir, dst_px, dst_baseline)

    out_mono, out_color = copy.deepcopy(mono), copy.deepcopy(color)
    for sub in ("mono", "color"):
        os.makedirs(os.path.join(work, sub, "glyphs"))
    counts = Counter()

    for gm, gc in zip(out_mono["glyphs"], out_color["glyphs"]):
        cp = gm["codepoint"]
        is_color = gc.get("color", False)
        mono_png = os.path.join(mono_dir, gm["file"])
        r = noto.render(NOTO_ALIASES.get(cp, cp), dst_px) if gm["width"] else None
        shift = 0
        if r:
            left, top, adv = r["left"], dst_baseline - r["top"], r["advance"]
            width, height = r["width"], r["height"]
            img = Image.new("1", (width, height), 1)
            img.putdata([0 if b else 1 for b in r["bits"]])
            shift = center_shift(gm, mono_png, top, img, src_size, args.size)
            top += shift
            counts["rendered from Noto Emoji"] += 1
        else:
            x0 = round(gm["left_offset"] * ratio)
            x1 = round((gm["left_offset"] + gm["width"]) * ratio)
            y0 = dst_baseline - round((src_baseline - gm["top_offset"]) * ratio)
            y1 = dst_baseline - round(
                (src_baseline - gm["top_offset"] - gm["height"]) * ratio
            )
            left, top, adv = x0, y0, round(gm["advance"] * ratio)
            width, height = x1 - x0, y1 - y0
            if cp in SHAPES:
                width, height = shape_size(
                    cp, gm["width"] * ratio, gm["height"] * ratio
                )
                left = round((x0 + x1 - width) / 2)
                top = round((y0 + y1 - height) / 2)
            if not gm["width"]:
                img = None
            elif cp in SHAPES:
                img = shape_image(cp, width, height, mono_png, False)
                counts["drawn from polygons"] += 1
            else:
                sys.exit(f"U+{cp:04X} isn't in Noto Emoji; add it to SHAPES")

        for g in (gm, gc):
            g.update(
                width=width,
                height=height,
                left_offset=left,
                top_offset=top,
                advance=adv,
            )
        if img is None or not width or not height:
            continue
        img.convert("1").save(os.path.join(work, "mono", gm["file"]))
        if cp in colored:
            img, left, top = colored[cp]
            gc.update(
                width=img.width,
                height=img.height,
                left_offset=left,
                top_offset=top - emoji_color.BASELINE + dst_baseline + shift,
                advance=r["advance"],
            )
            counts["colored from Noto Color Emoji"] += 1
        elif emoji_color_overrides.OV.get(cp, {}).get("bw"):
            # Keep the 1-bit glyph
            gc["color"] = False
        elif is_color and cp in SHAPES:
            color_png = os.path.join(color_dir, gc["file"])
            img = shape_image(cp, width, height, color_png, True)
            counts["in color"] += 1
        elif is_color:
            # Keep the 1-bit glyph; --noto-color colors these
            gc["color"] = False
        img.save(os.path.join(work, "color", gc["file"]))

    print(", ".join(f"{n} {what}" for what, n in counts.items()))

    name = f"EMOJI_{args.size}"
    for manifest, sub, out_name in (
        (out_mono, "mono", f"{name}.pbf"),
        (out_color, "color", f"{name}~color.pbf"),
    ):
        manifest["max_height"] = args.size
        manifest["source"] = out_name
        manifest["_manifest_path"] = os.path.join(work, sub, "font.json")
        with contextlib.redirect_stdout(io.StringIO()):
            ok = pbf_repack.build_pbf(manifest, os.path.join(args.output, out_name))
        if not ok:
            sys.exit(f"Failed to pack {out_name}")
        print(f"Wrote {os.path.join(args.output, out_name)}")


def main():
    parser = argparse.ArgumentParser(
        description="Build a larger emoji font from a smaller one"
    )
    parser.add_argument("mono", help="Source 1-bit emoji font, e.g. EMOJI_28.pbf")
    parser.add_argument("color", help="Its color variant, e.g. EMOJI_28~color.pbf")
    parser.add_argument("--noto", required=True, help="Noto Emoji TTF file")
    parser.add_argument(
        "--noto-color",
        help="Noto Color Emoji SVG directory, with emoji_u<codepoint>.svg files",
    )
    parser.add_argument("--size", type=int, required=True, help="Target font size")
    parser.add_argument("-o", "--output", required=True, help="Output directory")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as work:
        build(args, work)
    return 0


if __name__ == "__main__":
    sys.exit(main())
