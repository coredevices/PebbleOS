# SPDX-FileCopyrightText: 2026 Scott Anderson
# SPDX-License-Identifier: Apache-2.0

"""
Color a Noto Emoji 1-bit glyph from the Noto Color Emoji art.

The 1-bit lines stay. Each area they enclose takes the color the color art
mostly has there, picked from the colors the smaller color glyph uses, so
the glyph keeps the look of the smaller font. Lines that aren't needed for
contrast take the color next to them. Glyphs the rules don't get right have
overrides in emoji_color_overrides.

Glyphs are drawn in a 40x40 frame, Noto Emoji at 26 px with the baseline at
row 32; override coordinates are in this frame.
"""

import itertools
import math
import os
import subprocess
import tempfile
from collections import Counter

from PIL import Image, ImageChops, ImageDraw, ImageStat

PX, BASELINE, W, H = 26, 32, 40, 40
OX = (W - PX) // 2

N4 = ((1, 0), (-1, 0), (0, 1), (0, -1))
N8 = tuple((dx, dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1))
BLACK, WHITE, RED, FACE = (0, 0, 0), (255, 255, 255), (255, 0, 0), (255, 255, 85)
PALETTE = [
    (r * 85, g * 85, b * 85) for r in range(4) for g in range(4) for b in range(4)
]


def _lin(c):
    c /= 255
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def lab(rgb):
    r, g, b = (_lin(v) for v in rgb)
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883

    def f(t):
        return t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116

    fx, fy, fz = f(x), f(y), f(z)
    return (116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz))


def oklab(lab_c):
    """CIELAB to OKLab, scaled so its numbers are about CIELAB's size."""
    lightness, a, b = lab_c
    fy = (lightness + 16) / 116
    fx, fz = fy + a / 500, fy - b / 200

    def g(t):
        return t**3 if t**3 > 0.008856 else (t - 16 / 116) / 7.787

    x, y, z = g(fx) * 0.95047, g(fy), g(fz) * 1.08883
    lc = 0.8189330101 * x + 0.3618667424 * y - 0.1288597137 * z
    mc = 0.0329845436 * x + 0.9293118715 * y + 0.0361456387 * z
    sc = 0.0482003018 * x + 0.2643662691 * y + 0.6338517070 * z
    lc, mc, sc = (math.copysign(abs(v) ** (1 / 3), v) for v in (lc, mc, sc))
    return (
        100 * (0.2104542553 * lc + 0.7936177850 * mc - 0.0040720468 * sc),
        350 * (1.9779984951 * lc - 2.4285922050 * mc + 0.4505937099 * sc),
        350 * (0.0259040371 * lc + 0.7827717662 * mc - 0.8086757660 * sc),
    )


POK = {p: oklab(lab(p)) for p in PALETTE}


def nearest(rgb, colors):
    o = oklab(lab(rgb))
    return min(colors, key=lambda c: math.dist(o, POK[c]))


def frame(noto, cp):
    """The 1-bit glyph in the frame as rows of 0/1, or None."""
    r = noto.render(cp, PX)
    if not r or not r["width"]:
        return None
    m = [[0] * W for _ in range(H)]
    mx, my = OX + r["left"], BASELINE - r["top"]
    for y in range(r["height"]):
        for x in range(r["width"]):
            if r["bits"][y * r["width"] + x] and 0 <= my + y < H and 0 <= mx + x < W:
                m[my + y][mx + x] = 1
    return m


def outside(m):
    """Pixels reachable from the frame edge without crossing black (4-connected)."""
    out = [[False] * W for _ in range(H)]
    stack = [(x, y) for x in range(W) for y in (0, H - 1)]
    stack += [(x, y) for y in range(H) for x in (0, W - 1)]
    while stack:
        x, y = stack.pop()
        if 0 <= x < W and 0 <= y < H and not out[y][x] and not m[y][x]:
            out[y][x] = True
            stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    return out


def outside_closed(m, diagonal=True):
    """Like outside(), but gaps of up to 2 px in an outline don't let it in.
    Pixels next to the outside, diagonally too unless diagonal is False, are
    outside."""
    grown = [
        [
            any(
                0 <= x + dx < W and 0 <= y + dy < H and m[y + dy][x + dx]
                for dx in (-1, 0, 1)
                for dy in (-1, 0, 1)
            )
            for x in range(W)
        ]
        for y in range(H)
    ]
    out = outside(grown)
    edge = [
        (x, y)
        for y in range(H)
        for x in range(W)
        if not out[y][x]
        and not m[y][x]
        and any(
            0 <= x + dx < W and 0 <= y + dy < H and out[y + dy][x + dx]
            for dx, dy in (N8 if diagonal else N4)
        )
    ]
    for x, y in edge:
        out[y][x] = True
    return out


def bbox(mask):
    ys = [y for y in range(H) if any(mask[y])]
    xs = [x for x in range(W) if any(mask[y][x] for y in range(H))]
    return (min(xs), min(ys), max(xs) + 1, max(ys) + 1) if xs else None


def render_svg(path, size=260):
    with tempfile.TemporaryDirectory() as tmp:
        png = os.path.join(tmp, "art.png")
        subprocess.run(
            ["rsvg-convert", "-w", str(size), "-h", str(size), path, "-o", png],
            check=True,
        )
        return Image.open(png).convert("RGBA")


SS = 4  # alignment searches a 4x finer grid


def _fit(m, art, flip):
    out = outside_closed(m, diagonal=False)
    sil_mask = [[not out[y][x] for x in range(W)] for y in range(H)]
    sil = Image.new("L", (W, H))
    sil.putdata([255 if v else 0 for row in sil_mask for v in row])
    silf = sil.resize((W * SS, H * SS), Image.NEAREST)
    sil_area = sum(map(sum, sil_mask)) * SS * SS * 255
    x0, y0, x1, y1 = bbox(sil_mask)
    ab = art.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
    crop = art.crop(ab)
    if flip:
        crop = crop.transpose(Image.FLIP_LEFT_RIGHT)
    alpha = crop.getchannel("A")
    bw, bh = x1 - x0, y1 - y0
    cx, cy = (x0 + x1) / 2 * SS, (y0 + y1) / 2 * SS
    best = None
    for sx in (0.9, 0.95, 1.0, 1.05, 1.1):
        for sy in (0.9, 0.95, 1.0, 1.05, 1.1):
            w, h = max(1, round(bw * sx * SS)), max(1, round(bh * sy * SS))
            a = alpha.resize((w, h), Image.BOX)
            a_area = round(ImageStat.Stat(a).sum[0])
            for dy in range(-2 * SS, 2 * SS + 1, 2):
                for dx in range(-2 * SS, 2 * SS + 1, 2):
                    px, py = round(cx - w / 2) + dx, round(cy - h / 2) + dy
                    if px < 0 or py < 0 or px + w > W * SS or py + h > H * SS:
                        continue
                    region = silf.crop((px, py, px + w, py + h))
                    inter = round(ImageStat.Stat(ImageChops.multiply(a, region)).sum[0])
                    # Intersection over union, in units of 1/255
                    score = inter / (a_area + sil_area - inter)
                    if best is None or score > best[0]:
                        best = (score, px, py, w, h)
    score, px, py, w, h = best
    canvas = Image.new("RGBa", (W * SS, H * SS), (0, 0, 0, 0))
    canvas.paste(crop.convert("RGBa").resize((w, h), Image.BOX), (px, py))
    return canvas.resize((W, H), Image.BOX).convert("RGBA"), score


def align(m, art):
    """Fit the color art onto the 1-bit glyph's silhouette, by searching
    scale and offset for the best overlap, mirrored if that fits better."""
    img, score = _fit(m, art, False)
    flipped, fscore = _fit(m, art, True)
    return flipped if fscore > score + 0.05 else img


def _new_hue(c, colors):
    """A colorful hue the palette doesn't have, not a shade of one it does."""

    def hue(p):
        return math.atan2(POK[p][2], POK[p][1])

    def chroma(p):
        return math.hypot(POK[p][1], POK[p][2])

    if chroma(c) < 20:
        # Dark gray is shading; white and light gray are colors
        return POK[c][0] > 60
    return all(
        abs((hue(c) - hue(u) + math.pi) % (2 * math.pi) - math.pi) > 0.7
        for u in colors
        if chroma(u) >= 20
    )


def _accents(colors, art):
    """Up to 2 extra colors for a small palette, where the art has a large
    patch of a color far from all of the palette's."""
    if len(colors) > 3 or not art:
        return []
    share = Counter(nearest(c, PALETTE) for c in art)
    # An accent is a minority of the art; a majority would replace the main
    # color. Under 3 colors, smaller patches count too
    low = 0.03 if len(colors) < 3 else 0.1
    far = [
        c
        for c, n in share.most_common()
        if low * len(art) <= n <= 0.4 * len(art)
        and min(math.dist(POK[c], POK[u]) for u in colors) > 60
        and _new_hue(c, colors)
    ]
    return far[:2]


def _contrast(c):
    """Contrast ratio against white."""
    lin = [
        v / 255 / 12.92 if v <= 10 else ((v / 255 + 0.055) / 1.055) ** 2.4 for v in c
    ]
    return 1.05 / (0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2] + 0.05)


def _bordered(ref):
    """Whether the smaller glyph has a black outline around its edge."""
    px, (w, h) = ref.load(), ref.size
    edge = Counter()
    for y in range(h):
        for x in range(w):
            if px[x, y][3] and any(
                not (0 <= x + dx < w and 0 <= y + dy < h) or not px[x + dx, y + dy][3]
                for dx, dy in N4
            ):
                edge[px[x, y][:3] == BLACK] += 1
    return edge[True] >= edge[False]


def _flood(start, ok):
    seen, todo = {start}, [start]
    while todo:
        x, y = todo.pop()
        for dx, dy in N4:
            q = (x + dx, y + dy)
            if q not in seen and ok(*q):
                seen.add(q)
                todo.append(q)
    return seen


def _no_entry(m):
    """Black edge, thick red ring, white middle and a red slash, on the 1-bit
    circle."""
    x0, y0, x1, y1 = bbox(m)
    cx, cy, r = (x0 + x1 - 1) / 2, (y0 + y1 - 1) / 2, (x1 - x0) / 2
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    px = img.load()
    for y in range(H):
        for x in range(W):
            d = math.hypot(x - cx, y - cy)
            band = abs((x - cx) - (y - cy)) / 2**0.5
            if d > r:
                continue
            if d > r - 1.2:
                c = BLACK
            elif d > r - 5 or band < 2.2:
                c = RED
            else:
                c = WHITE
            px[x, y] = c + (255,)
    return img


def _art_only(art, ov):
    """Straight conversion of the color art to the palette."""
    img = Image.new("RGBA", art.size, (0, 0, 0, 0))
    for y in range(art.height):
        for x in range(art.width):
            a = art.getpixel((x, y))
            if a[3] >= 128:
                img.putpixel((x, y), nearest(a[:3], PALETTE) + (255,))
    # Repeat a row down, shifted left: (source row, [(row, shift)])
    for sy, rows in ov.get("extend", []):
        src = [img.getpixel((x, sy)) for x in range(art.width)]
        for y, s in rows:
            for x in range(art.width):
                img.putpixel((x, y), src[x + s] if x + s < art.width else (0, 0, 0, 0))
    return img


def colorize(m, art, ref, ov):
    """Color the 1-bit frame m from the aligned art. ref is the smaller color
    glyph, whose colors are used; ov are overrides. Returns an RGBA frame."""
    if ov.get("no_entry"):
        return _no_entry(m)
    if ov.get("art"):
        return _art_only(art, ov)
    m0 = m = [row[:] for row in m]
    A = art.load()
    # Close open ends with a line
    for x0, y0, x1, y1 in ov.get("close", []):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                m[y][x] = 1
    colors = ov.get("colors") or {p[:3] for p in ref.getdata() if p[3]}
    if colors == "all":
        colors = PALETTE
    extra = []
    if not ov.get("colors"):
        extra = _accents(
            colors,
            [A[x, y][:3] for y in range(H) for x in range(W) if A[x, y][3] >= 170],
        )

    def pick(rgb):
        # Art colors by nearest Pebble color, mapped to the color with that role
        if nearest(rgb, PALETTE) in ov.get("classes", {}):
            return ov["classes"][nearest(rgb, PALETTE)]
        # An accent only where the art is that color
        if extra and nearest(rgb, PALETTE) in extra:
            return nearest(rgb, PALETTE)
        return nearest(rgb, colors)

    def art_at(x, y):
        return A[x, y][:3] if A[x, y][3] >= 170 else None

    strict = outside(m)
    closed = outside_closed(m)
    out = [
        [
            strict[y][x]
            and (closed[y][x] or A[x, y][3] < 128 or x in (0, W - 1) or y in (0, H - 1))
            for x in range(W)
        ]
        for y in range(H)
    ]

    # Closed gaps next to the outside are drawn as line
    def near_out(x, y):
        return any(
            0 <= x + dx < W and 0 <= y + dy < H and out[y + dy][x + dx]
            for dx in (-1, 0, 1)
            for dy in (-1, 0, 1)
        )

    m = [
        [
            m[y][x] or (strict[y][x] and not out[y][x] and near_out(x, y))
            for x in range(W)
        ]
        for y in range(H)
    ]

    def inside(x, y):
        return 0 <= x < W and 0 <= y < H and not out[y][x]

    def free(x, y):
        return inside(x, y) and not m[y][x]

    col = {}
    seen, areas = set(), []
    for y in range(H):
        for x in range(W):
            if (x, y) in seen or not free(x, y):
                continue
            area = _flood((x, y), free)
            seen |= area
            areas.append(area)
            votes = Counter(pick(art_at(*p)) for p in area if art_at(*p))
            if not votes:
                # Translucent art, like glass, still votes
                glass = [A[q][:3] for q in area if A[q][3] >= 64]
                if len(glass) * 2 > len(area):
                    votes = Counter(pick(c) for c in glass)
            if len(votes) > 1 and votes[BLACK] < 0.6 * sum(votes.values()):
                del votes[BLACK]
            # An area the art doesn't paint is white, or empty if the palette
            # has no white
            c = ov.get("area") or (
                votes.most_common(1)[0][0]
                if votes
                else WHITE
                if WHITE in colors
                else None
            )
            for p in area:
                col[p] = c
    if ov.get("nested"):
        # Wire drawn as nested loops: areas alternate wire and hole, counting
        # the lines crossed from the outside
        area_of = {p: i for i, a in enumerate(areas) for p in a}
        depth = {}
        todo = [(x, y) for y in range(H) for x in range(W) if out[y][x]]
        level = 0
        while todo and level <= 20:
            nxt = set()
            for x, y in todo:
                for dx, dy in N4:
                    for k in (1, 2, 3):
                        q = (x + dx * k, y + dy * k)
                        if q in area_of:
                            if area_of[q] not in depth:
                                depth[area_of[q]] = level + 1
                                nxt |= set(areas[area_of[q]])
                            break
                        if not (0 <= q[0] < W and 0 <= q[1] < H and m[q[1]][q[0]]):
                            break
            todo, level = list(nxt), level + 1
        seq = ov["nested"]
        for i, a in enumerate(areas):
            c = seq[(depth.get(i, 1) - 1) % len(seq)]
            for p in a:
                col[p] = c
    # Solid black shapes the art paints are areas too; with solid, so is
    # hatching inside the outline
    if not ov.get("black"):

        def core(x, y):
            return (
                inside(x, y)
                and m[y][x]
                and (
                    all(
                        0 <= x + dx < W and 0 <= y + dy < H and m[y + dy][x + dx]
                        for dx, dy in N4
                    )
                    or (
                        ov.get("solid") and all(inside(x + dx, y + dy) for dx, dy in N4)
                    )
                )
            )

        for y in range(H):
            for x in range(W):
                if (x, y) in seen or not core(x, y):
                    continue
                blob = _flood((x, y), core)
                seen |= blob
                raw = Counter(art_at(*p) for p in blob).most_common(1)[0][0]
                if raw and (max(raw) > 85 or ov.get("solid")):
                    votes = Counter(pick(art_at(*p)) for p in blob if art_at(*p))
                    if len(votes) > 1 and votes[BLACK] < 0.6 * sum(votes.values()):
                        del votes[BLACK]
                    for p in blob:
                        col[p] = votes.most_common(1)[0][0]
    # A black patch floating inside colored areas is a color the art paints
    # there
    if not ov.get("black"):

        def blk(x, y):
            return inside(x, y) and (x, y) not in col

        for y in range(H):
            for x in range(W):
                if (x, y) in seen or not blk(x, y):
                    continue
                blob = _flood((x, y), blk)
                seen |= blob
                ring = {(a + dx, b + dy) for a, b in blob for dx, dy in N4} - blob
                if any(col.get(q) is None for q in ring):
                    continue
                votes = Counter(pick(art_at(*p)) for p in blob if art_at(*p))
                if not votes:
                    continue
                c, n = votes.most_common(1)[0]
                around = Counter(col[q] for q in ring).most_common(1)[0][0]
                # Light detail inside a black area, like keys on a phone, shows
                # white
                if around == BLACK:
                    light = [p for p in blob if art_at(*p) and lab(art_at(*p))[0] > 50]
                    if 2 * len(light) <= len(blob):
                        continue
                    c, n = WHITE, len(blob)
                # Gray is shading on a line, not a color of its own
                gray = max(c) - min(c) < 40
                if (
                    c not in (BLACK, around)
                    and not (gray and around != BLACK)
                    and 2 * n > len(blob)
                ):
                    for p in blob:
                        col[p] = c
    # Seeded fills: the 4-connected area at a point takes a color, None
    # empties it
    for (x, y), c in ov.get("fill", {}).items():
        target = col.get((x, y))
        for p in _flood(
            (x, y), lambda a, b, t=target: free(a, b) and col.get((a, b)) == t
        ):
            col[p] = c
    if ov.get("blobs"):
        # Solid black shapes, any pixel in a 2x2 black square, take a color
        def sq(x, y):
            return any(
                all(
                    0 <= x + a < W and 0 <= y + b < H and m[y + b][x + a]
                    for a in (i, i + 1)
                    for b in (j, j + 1)
                )
                for i in (-1, 0)
                for j in (-1, 0)
            )

        for y in range(H):
            for x in range(W):
                if inside(x, y) and m[y][x] and sq(x, y):
                    col[(x, y)] = ov["blobs"]
    if ov.get("pixels"):
        # Each colored pixel takes its own art color, then the most common
        # color around it so shading doesn't speckle
        own = {
            q: pick(art_at(*q))
            for q, c in col.items()
            if c not in (None, BLACK) and art_at(*q)
        }
        for (x, y), here in own.items():
            near = Counter(
                own[(x + dx, y + dy)]
                for dx in (-1, 0, 1)
                for dy in (-1, 0, 1)
                if (x + dx, y + dy) in own
            )
            col[(x, y)] = (
                near.most_common(1)[0][0] if ov["pixels"] == "smooth" else here
            )
    fills = {c for c in col.values() if c not in (None, BLACK)}
    if len(fills) <= 1 and not ov.get("own"):
        # A glyph left with one color takes the art's other colors where the
        # art has them, in fills and in solid black shapes like hair
        main = set(fills) | {BLACK}
        allart = [art_at(x, y) for y in range(H) for x in range(W) if art_at(x, y)]
        share = Counter(nearest(c, PALETTE) for c in allart)
        new = [
            c
            for c, n in share.most_common()
            if n >= 0.015 * len(allart)
            and _new_hue(c, main)
            and min(math.dist(POK[c], POK[u]) for u in main) > 60
        ]
        if new:
            ov = dict(ov, own=new[:2], own_lines="only")
    if ov.get("own"):
        # Pixels where the art is one of these colors take it, so a patch
        # follows the art's shape across lines; lines too with own_lines
        cs, lines = ov["own"], ov.get("own_lines")

        def hit(q):
            return art_at(*q) and nearest(art_at(*q), list(colors) + list(cs)) in cs

        mine = set()
        if lines != "only":
            mine = {
                q
                for q in col
                if col[q] not in (None, WHITE) and (lines or col[q] != BLACK) and hit(q)
            }
        if lines:
            mine |= {
                (x, y)
                for y in range(H)
                for x in range(W)
                if inside(x, y)
                and m[y][x]
                and (x, y) not in col
                and hit((x, y))
                and all(col.get((x + dx, y + dy)) != WHITE for dx, dy in N4)
            }
        if lines == "only":
            # Black shapes only, where most of the 3x3 around agrees
            mine = {
                (x, y)
                for x, y in mine
                if sum(
                    (x + dx, y + dy) in mine for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                )
                >= 5
            }
        for q in mine:
            col[q] = nearest(art_at(*q), cs)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    px = img.load()
    for y in range(H):
        for x in range(W):
            if inside(x, y):
                c = col.get((x, y), BLACK)
                # The outline, black pixels touching the outside, stays black
                if m[y][x] and any(
                    not inside(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                ):
                    c = BLACK
                if c is not None:
                    px[x, y] = c + (255,)
    # A black line between white or the background and a color that stands
    # out against white on its own isn't needed: it takes that color
    if not ov.get("lined"):
        # White counts as background when it's a large area, not a highlight
        def wh(x, y):
            return 0 <= x < W and 0 <= y < H and px[x, y] == WHITE + (255,)

        big, seen = set(), set()
        for y in range(H):
            for x in range(W):
                if wh(x, y) and (x, y) not in seen:
                    a = _flood((x, y), wh)
                    seen |= a
                    if len(a) >= 16:
                        big |= a
        snap = img.copy().load()

        def get(x, y):
            return snap[x, y] if 0 <= x < W and 0 <= y < H else (0, 0, 0, 0)

        # The edge of the glyph keeps its outline when the smaller glyph has
        # one
        edge_ok = not _bordered(ref)

        def isbg(x, y):
            return (edge_ok and not get(x, y)[3]) or (x, y) in big

        def blk(x, y):
            return get(x, y) == BLACK + (255,)

        # One pass over the original, so a line between two pieces of one
        # color isn't eaten from its ends
        for y in range(H):
            for x in range(W):
                if not blk(x, y):
                    continue
                near = {
                    get(x + dx, y + dy)[:3]
                    for dx in (-1, 0, 1)
                    for dy in (-1, 0, 1)
                    if not isbg(x + dx, y + dy)
                } - {BLACK}
                if len(near) != 1:
                    continue
                c = near.pop()
                if _contrast(c) < 2.4:
                    continue
                # The color on one side and the background straight across,
                # through a line up to 2 px thick
                for dx, dy in N4:
                    side = get(x + dx, y + dy)[:3] == c or (
                        blk(x + dx, y + dy)
                        and get(x + 2 * dx, y + 2 * dy)[:3] == c
                        and isbg(x - dx, y - dy)
                    )
                    across = isbg(x - dx, y - dy) or (
                        blk(x - dx, y - dy)
                        and isbg(x - 2 * dx, y - 2 * dy)
                        and get(x + dx, y + dy)[:3] == c
                    )
                    if side and across:
                        px[x, y] = c + (255,)
                        break
    _overrides(img, ov, m0, A, inside)
    return img


def _box(x0, y0, x1, y1):
    return [(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)]


def _snap(px):
    return {(x, y): px[x, y] for y in range(H) for x in range(W)}


def _overrides(img, ov, m0, A, inside):
    """Overrides drawn over the colored glyph, in this order."""
    px = img.load()
    if ov.get("unlined"):
        # No black lines: each takes a neighbor's color
        while True:
            todo = {}
            for y in range(H):
                for x in range(W):
                    if px[x, y] == BLACK + (255,):
                        near = Counter(
                            px[x + dx, y + dy][:3]
                            for dx, dy in N4
                            if 0 <= x + dx < W
                            and 0 <= y + dy < H
                            and px[x + dx, y + dy][3]
                            and px[x + dx, y + dy][:3] != BLACK
                        )
                        if near:
                            todo[(x, y)] = near.most_common(1)[0][0]
            if not todo:
                break
            for q, c in todo.items():
                px[q] = c + (255,)
    # A glyph drawn only in strokes takes its color in the strokes
    if ov.get("ink"):
        for q in _box(0, 0, W - 1, H - 1):
            if px[q] == BLACK + (255,):
                px[q] = ov["ink"] + (255,)
    # Thicken strokes, by default one pixel right and down
    if ov.get("thick"):
        offs = (
            ov["thick"] if isinstance(ov["thick"], list) else ((1, 0), (0, 1), (1, 1))
        )
        for x, y in [q for q in _box(0, 0, W - 1, H - 1) if px[q][3]]:
            for q in [(x + dx, y + dy) for dx, dy in offs]:
                if q[0] < W and q[1] < H and not px[q][3]:
                    px[q] = px[x, y]
    # Shapes with no 1-bit area: ('ring', cx, cy, r0, r1, color) or
    # ('line', x0, y0, x1, y1, width, color)
    for shape in ov.get("draw", []):
        for x, y in _box(0, 0, W - 1, H - 1):
            if not px[x, y][3]:
                continue
            if shape[0] == "ring":
                _, cx, cy, r0, r1, c = shape
                hit = r0 <= math.hypot(x - cx, y - cy) <= r1
            else:
                _, x0, y0, x1, y1, w, c = shape
                dx, dy = x1 - x0, y1 - y0
                t = max(
                    0, min(1, ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy))
                )
                hit = math.hypot(x - x0 - t * dx, y - y0 - t * dy) <= w / 2
            if hit:
                px[x, y] = c + (255,)
    # Start over from the bare 1-bit lines
    if ov.get("mono"):
        for x, y in _box(0, 0, W - 1, H - 1):
            px[x, y] = BLACK + (255,) if m0[y][x] else (0, 0, 0, 0)
    # Pockets: empty pixels an 8-way flood from the edge can't reach take
    # (default color, [(box, color)])
    if ov.get("pockets"):
        dflt, boxes = ov["pockets"]
        out = set()
        todo = [(x, y) for x in range(W) for y in (0, H - 1)]
        todo += [(x, y) for y in range(H) for x in (0, W - 1)]
        while todo:
            x, y = todo.pop()
            if not (0 <= x < W and 0 <= y < H) or (x, y) in out or px[x, y][3]:
                continue
            out.add((x, y))
            todo += [(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)]
        for x, y in _box(0, 0, W - 1, H - 1):
            if (x, y) not in out and not px[x, y][3]:
                c = next(
                    (
                        bc
                        for (x0, y0, x1, y1), bc in boxes
                        if x0 <= x <= x1 and y0 <= y <= y1
                    ),
                    dflt,
                )
                px[x, y] = c + (255,)
    # Inclusive boxes (x0, y0, x1, y1, color, which); which is all, fill,
    # line, unlined, lined, inner, or a color to replace
    for x0, y0, x1, y1, c, which in ov.get("paint", []):
        for x, y in _box(x0, y0, x1, y1):
            p = px[x, y]
            if not p[3]:
                continue
            if (
                which == "all"
                or (which == "fill" and p[:3] != BLACK)
                or (which == "line" and m0[y][x])
                or (which == "unlined" and not m0[y][x])
                or (which == "lined" and m0[y][x] and p[:3] != BLACK)
                or (
                    which == "inner"
                    and m0[y][x]
                    and p[:3] == BLACK
                    and all(
                        inside(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                    )
                )
                or which == p[:3]
            ):
                px[x, y] = c + (255,) if c else (0, 0, 0, 0)
    # Open areas: (x, y, color, box or [boxes]) floods empty pixels in the box
    for sx, sy, c, box in ov.get("flood", []):
        boxes = box if isinstance(box, list) else [box]
        todo, seen = [(sx, sy)], set()
        while todo:
            x, y = todo.pop()
            if (
                (x, y) in seen
                or px[x, y][3]
                or not any(x0 <= x <= x1 and y0 <= y <= y1 for x0, y0, x1, y1 in boxes)
            ):
                continue
            seen.add((x, y))
            todo += [(x + dx, y + dy) for dx, dy in N4]
        for q in seen:
            px[q] = c + (255,)
    # Recolor from the art, keeping black on the edge: artfill colors
    if ov.get("artfill"):
        cols = ov["artfill"]
        for x, y in _box(0, 0, W - 1, H - 1):
            if not inside(x, y) or (
                m0[y][x] and not all(inside(x + dx, y + dy) for dx, dy in N4)
            ):
                continue
            a = A[x, y]
            px[x, y] = (WHITE if a[3] < 128 else nearest(a[:3], cols)) + (255,)
    # Bitmap text: (x, y, rows, color)
    for tx, ty, rows, c in ov.get("text", []):
        for dy, row in enumerate(rows):
            for dx, ch in enumerate(row):
                if ch == "#":
                    px[tx + dx, ty + dy] = c + (255,)
    # Hair: black turns to the hair color unless it only borders the face
    # (eyes, mouth, face outline)
    if ov.get("hair"):
        hair, face = ov["hair"], FACE

        def near(snap, x, y, c):
            return any(
                snap.get((x + dx, y + dy), (0, 0, 0, 0)) == c + (255,)
                for dx in (-1, 0, 1)
                for dy in (-1, 0, 1)
            )

        snap = _snap(px)
        for (x, y), p in snap.items():
            if p == BLACK + (255,) and not near(snap, x, y, face):
                px[x, y] = hair + (255,)
        snap = _snap(px)
        for (x, y), p in snap.items():
            if p == BLACK + (255,) and near(snap, x, y, hair):
                px[x, y] = hair + (255,)
    # Black pixels take their most common colored neighbor: 'all', or 'inner'
    # to keep the black edge
    if ov.get("noblack"):
        for _ in range(3):
            snap = _snap(px)
            for (x, y), p in snap.items():
                if p != BLACK + (255,):
                    continue
                nb = [
                    snap.get((x + dx, y + dy), (0, 0, 0, 0))
                    for dx in (-1, 0, 1)
                    for dy in (-1, 0, 1)
                ]
                if ov["noblack"] == "inner" and any(not q[3] for q in nb):
                    continue
                cols = Counter(q[:3] for q in nb if q[3] and q[:3] != BLACK)
                if cols:
                    px[x, y] = cols.most_common(1)[0][0] + (255,)
    # Black lines between two colors: [(color, color, into)]
    for c1, c2, into in ov.get("between", []):
        snap = _snap(px)
        for (x, y), p in snap.items():
            if p != BLACK + (255,):
                continue
            nb = {
                snap.get((x + dx, y + dy), (0, 0, 0, 0))
                for dx in (-1, 0, 1)
                for dy in (-1, 0, 1)
            }
            if c1 + (255,) in nb and c2 + (255,) in nb and all(q[3] for q in nb):
                px[x, y] = into + (255,)
    # Paint off the 1-bit lines inside polygons: [(points, color)]
    for points, c in ov.get("polygon", []):
        mask = Image.new("1", (W, H))
        ImageDraw.Draw(mask).polygon(points, fill=1)
        for x, y in _box(0, 0, W - 1, H - 1):
            if mask.getpixel((x, y)) and not m0[y][x]:
                px[x, y] = c + (255,)
    # Recolor non-black pixels where the art is one of some colors:
    # [(box, art colors, color)]
    for box, artc, c in ov.get("artrecolor", []):
        for x, y in _box(*box):
            p = px[x, y]
            if (
                p[3]
                and p[:3] != BLACK
                and A[x, y][3] >= 128
                and nearest(A[x, y][:3], PALETTE) in artc
            ):
                px[x, y] = c + (255,)
    # Shading: non-black pixels where an art channel is below a value:
    # [(box, channel, value, color)]
    for box, ch, v, c in ov.get("artbelow", []):
        for x, y in _box(*box):
            p = px[x, y]
            if p[3] and p[:3] != BLACK and A[x, y][3] >= 128 and A[x, y][ch] < v:
                px[x, y] = c + (255,)
    # Polylines over everything: [(points, width, color)]
    for pts, w, c in ov.get("lines", []):
        for (ax, ay), (bx, by) in itertools.pairwise(pts):
            dx, dy = bx - ax, by - ay
            for x, y in _box(0, 0, W - 1, H - 1):
                t = max(
                    0, min(1, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy))
                )
                if math.hypot(x - ax - t * dx, y - ay - t * dy) <= w / 2:
                    px[x, y] = c + (255,)
