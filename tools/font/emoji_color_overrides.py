# SPDX-FileCopyrightText: 2026 Scott Anderson
# SPDX-License-Identifier: Apache-2.0

"""Per-glyph overrides for emoji_color, in its 40x40 frame."""

import re

from pebble_colors import (
    ARMY_GREEN,
    BABY_BLUE_EYES,
    BLACK,
    BLUE,
    BRASS,
    BRILLIANT_ROSE,
    BULGARIAN_ROSE,
    CADET_BLUE,
    CELESTE,
    CHROME_YELLOW,
    COBALT_BLUE,
    DARK_CANDY_APPLE_RED,
    DARK_GRAY,
    DARK_GREEN,
    ICTERINE,
    INCHWORM,
    ISLAMIC_GREEN,
    JAZZBERRY_JAM,
    KELLY_GREEN,
    LIGHT_GRAY,
    LIMERICK,
    MAY_GREEN,
    MELON,
    MIDNIGHT_GREEN,
    ORANGE,
    PASTEL_YELLOW,
    PICTON_BLUE,
    RAJAH,
    RED,
    ROSE_VALE,
    SUNSET_ORANGE,
    TIFFANY_BLUE,
    VIVID_CERULEAN,
    VIVID_VIOLET,
    WHITE,
    WINDSOR_TAN,
    YELLOW,
)

JULY = [
    "..#.....#....",
    "..#.....#....",
    "..#.#.#.#.#.#",
    "#.#.#.#.#.#.#",
    ".#..###.#.###",
    "............#",
    "..........##.",
]
# Hatching in the 1-bit glyph marks color: fill the whole shape
SOLID = {"solid": True}
# cp -> overrides; emoji_color says what each key does where it reads it.
# Colors are Pebble colors from pebble_colors.
OV = {
    **{
        cp: SOLID
        for cp in (
            0x2764,
            0x1F534,
            0x1F7E2,
            0x1F4D5,
            0x1F4D7,
            0x1F4D9,
        )
    },
    # Hearts: no black at all
    **{
        cp: {"noblack": "all"}
        for cp in (0x1F499, 0x1F49A, 0x1F49B, 0x1F49C, 0x1F90E, 0x2666)
    },
    # White flag: the 1-bit glyph as is
    0x1F3F3: {"bw": True},
    # Rain and snow: everything below the cloud blue
    **{
        cp: {"paint": [(0, 26, 39, 39, PICTON_BLUE, "all")]}
        for cp in (0x1F327, 0x1F328)
    },
    # Holding back tears: eyes from the art, white above blue tears, black
    # where the tears meet the face
    0x1F979: {
        "artrecolor": [
            ((14, 19, 33, 27), [WHITE, LIGHT_GRAY], WHITE),
            (
                (14, 19, 33, 27),
                [VIVID_CERULEAN, CADET_BLUE, PICTON_BLUE, TIFFANY_BLUE, MIDNIGHT_GREEN],
                VIVID_CERULEAN,
            ),
            ((14, 19, 33, 27), [DARK_GRAY], BLACK),
        ],
        "paint": [
            (31, 21, 31, 24, BLACK, "all"),
            (31, 25, 31, 25, VIVID_CERULEAN, "all"),
        ],
        # A white and gray glint, as in the art
        "lines": [
            ([(29, 24), (29, 24.1)], 1, WHITE),
            ([(30, 24), (30, 24.1)], 1, LIGHT_GRAY),
        ],
    },
    # Black heart: solid black
    0x1F5A4: {"area": BLACK},
    # Hatched hearts: solid, no black
    **{
        cp: {"solid": True, "noblack": "all"}
        for cp in (0x1F9E1, 0x1FA75, 0x1FA76, 0x1FA77)
    },
    # Pizza: yellow cheese, red pepperoni
    0x1F355: {
        "classes": {RAJAH: ICTERINE, ORANGE: ICTERINE},
        "blobs": RED,
    },
    # Donut: pink frosting on light orange dough
    0x1F369: {
        "classes": {ROSE_VALE: BRILLIANT_ROSE},
        "area": WHITE,
        "blobs": BRILLIANT_ROSE,
        "fill": {(22, 20): None, (20, 30): RAJAH},
    },
    # Fear and crying: the blue follows the art
    **{cp: {"own": [BABY_BLUE_EYES], "own_lines": True} for cp in (0x1F628, 0x1F631)},
    0x1F62D: {"own": [VIVID_CERULEAN]},
    # Burger: green lettuce
    0x1F354: {"classes": {LIMERICK: INCHWORM}},
    # Ballot box with check: blue box
    0x2611: {
        "classes": {LIGHT_GRAY: PICTON_BLUE},
        "noblack": "inner",
    },
    # Koala: both eyes solid black
    0x1F428: {"paint": [(28, 22, 28, 23, BLACK, "all")]},
    # Recycling: light green, no lines
    0x267B: {"classes": {MAY_GREEN: INCHWORM}, "unlined": True},
    # Alarm clock: the 1-bit rim is open at the bottom, so it's drawn
    0x23F0: {"draw": [("ring", 23.5, 24.5, 9, 11.6, RED)]},
    # Guitar: red body, white pickguard, brown neck
    0x1F3B8: {
        "classes": {
            MELON: RED,
            ROSE_VALE: RED,
            LIGHT_GRAY: WHITE,
            WHITE: WHITE,
        },
        "pixels": "smooth",
        "lined": True,
    },
    # Zzz: blue strokes, thickened since thin colored strokes read poorly
    0x1F4A4: {"ink": PICTON_BLUE, "thick": True},
    # DVD: white ring around the hole
    0x1F4C0: {"fill": {(20, 20): WHITE}},
    # Rocket: a solid black window
    0x1F680: {"paint": [(26, 19, 27, 20, BLACK, "all")]},
    # Cold face: blue face like the art
    0x1F976: {"classes": {CELESTE: PICTON_BLUE, VIVID_CERULEAN: PICTON_BLUE}},
    # Snowflake: solid blue like the 28 pt glyph
    0x2744: {"area": PICTON_BLUE, "ink": PICTON_BLUE},
    # Jack-o'-lantern: orange with yellow cutouts, black ribs
    0x1F383: {"classes": {ORANGE: CHROME_YELLOW, RAJAH: CHROME_YELLOW}},
    # Sushi: dark pink stripes on the salmon
    0x1F363: {
        "between": [(SUNSET_ORANGE, WHITE, SUNSET_ORANGE)],
        "lined": True,
        "paint": [
            (14, 10, 38, 17, RAJAH, "inner"),
            (26, 18, 34, 19, RAJAH, BLACK),
        ],
    },
    # Hundred points: red strokes, thickened since thin colored strokes read
    # poorly, no fill
    0x1F4AF: {"area": WHITE, "ink": RED, "thick": [(1, 0)]},
    # Calendars: white month name
    0x1F4C5: {
        "paint": [(14, 11, 32, 19, RED, "all")],
        "text": [(17, 12, JULY, WHITE)],
    },
    0x1F4C6: {
        "paint": [(13, 12, 28, 19, RED, "all"), (13, 14, 32, 19, RED, "all")],
        "text": [(16, 12, JULY, WHITE)],
    },
    # Rainbow: six bands
    0x1F308: {
        "art": True,
        "extend": [(30, [(31, 0), (32, 1), (33, 1), (34, 1), (35, 1)])],
    },
    # Popcorn: red and white stripes
    0x1F37F: {
        "paint": [(13, 18, 35, 37, WHITE, "fill"), (13, 18, 35, 37, WHITE, "inner")]
        + [(x, 18, x + 1, 37, RED, WHITE) for x in (15, 19, 23, 27, 31)]
        + [(14, 17, 33, 17, BLACK, "all")]
    },
    # Broken heart: all red, black only around the edge
    0x1F494: {
        "paint": [
            (19, 28, 22, 35, RED, BLACK),
            (22, 13, 26, 35, WHITE, BLACK),
        ]
    },
    # Heart with ribbon: no black around the bow
    0x1F49D: {"paint": [(8, 12, 38, 28, ORANGE, BLACK), (0, 29, 39, 39, RED, BLACK)]},
    # Wastebasket: blue, black only around the edge
    0x1F5D1: {"artfill": [PICTON_BLUE]},
    # Star-struck: yellow face, red stars, white teeth
    0x1F929: {
        "classes": {
            CHROME_YELLOW: ICTERINE,
            SUNSET_ORANGE: RED,
            LIGHT_GRAY: WHITE,
            ARMY_GREEN: BULGARIAN_ROSE,
        },
        "paint": [
            (11, 13, 14, 17, ICTERINE, RED),
            (32, 13, 35, 17, ICTERINE, RED),
            (12, 25, 15, 27, ICTERINE, BULGARIAN_ROSE),
        ],
    },
    # Partying face: yellow face
    0x1F973: {"classes": {CHROME_YELLOW: ICTERINE, ARMY_GREEN: ICTERINE}},
    # Blossom: yellow petals
    0x1F33C: {"classes": {CHROME_YELLOW: ICTERINE}},
    # Cheese: yellow wedge, pale top, white holes
    0x1F9C0: {
        "classes": {
            CHROME_YELLOW: ICTERINE,
            RAJAH: ICTERINE,
        },
        "paint": [(0, 0, 39, 39, WHITE, "lined")],
    },
    # Egg: pale speckles
    0x1F95A: {"paint": [(0, 0, 39, 39, PASTEL_YELLOW, "inner")]},
    # Charts: colored trend line
    0x1F4C8: {
        "paint": [(0, 0, 39, 39, LIGHT_GRAY, "inner")],
        "lines": [([(11, 34), (16, 24), (21, 29), (34, 11)], 2, RED)],
    },
    0x1F4C9: {
        "paint": [(0, 0, 39, 39, LIGHT_GRAY, "inner")],
        "lines": [([(11, 11), (16, 20), (21, 15), (34, 34)], 2, COBALT_BLUE)],
    },
    # Question and exclamation marks: all red
    0x2753: {"noblack": "all"},
    0x2757: {"noblack": "all"},
    # Paperclip: empty inside the bottom loop
    0x1F4CE: {"fill": {(30, 28): None}},
    # Lock with key: empty inside the shackle
    0x1F510: {"fill": {(18, 15): None}},
    0x1F495: {"noblack": "all"},
    0x1F496: {"noblack": "all"},
    0x1F497: {"paint": [(0, 0, 39, 39, JAZZBERRY_JAM, BLACK)]},
    # Waffle: yellow butter
    0x1F9C7: {"artrecolor": [((0, 0, 39, 39), [ICTERINE, PASTEL_YELLOW], ICTERINE)]},
    # Beating heart: red heart, pink motion lines
    0x1F493: {
        "classes": {BRILLIANT_ROSE: RED},
        "ink": BRILLIANT_ROSE,
    },
    # Muted speaker: like the muted bell, blue speaker in a red circle and slash
    0x1F507: {
        "area": WHITE,
        "black": True,
        "fill": {(19, 21): PICTON_BLUE},
        "paint": [(18, 14, 28, 30, COBALT_BLUE, "inner")],
        "draw": [
            ("ring", 22.5, 22.5, 10.6, 14, RED),
            ("line", 13, 14, 32, 33, 1.6, RED),
        ],
    },
    # Camera: dark gray body, blue lens
    0x1F4F7: {
        "blobs": DARK_GRAY,
        "paint": [
            (22, 22, 30, 30, COBALT_BLUE, DARK_GRAY),
        ],
    },
    # E-mail: dark blue E
    0x1F4E7: {
        "paint": [(20, 18, 27, 27, WHITE, "all")],
        "text": [
            (
                20,
                18,
                ["#######"] * 2
                + ["##....."] * 2
                + ["######."] * 2
                + ["##....."] * 2
                + ["#######"] * 2,
                COBALT_BLUE,
            )
        ],
    },
    # Folded hands: blue sleeves, the outer corners cut off
    0x1F64F: {
        "polygon": [
            ([(11, 28), (17, 28), (18, 31), (19, 35), (18, 35)], COBALT_BLUE),
            ([(35, 28), (29, 28), (28, 31), (27, 35), (28, 35)], COBALT_BLUE),
        ]
    },
    # Brown-haired people: hair, beard and their outlines all brown
    0x1F466: {"hair": WINDSOR_TAN},
    0x1F467: {"hair": WINDSOR_TAN},
    0x1F468: {"hair": WINDSOR_TAN},
    0x1F469: {"hair": WINDSOR_TAN},
    0x1F9D1: {"hair": WINDSOR_TAN},
    0x1F9D2: {"hair": WINDSOR_TAN},
    0x1F9D4: {
        "hair": WINDSOR_TAN,
        "paint": [(17, 23, 30, 25, WINDSOR_TAN, BLACK)],
    },
    # Sunflower: yellow petals, brown middle, green leaves
    0x1F33B: {
        "classes": {
            CHROME_YELLOW: ICTERINE,
            DARK_GRAY: WINDSOR_TAN,
            ROSE_VALE: WINDSOR_TAN,
            LIMERICK: ISLAMIC_GREEN,
        }
    },
    # Palm tree: green leaves
    0x1F334: {
        "classes": {
            ARMY_GREEN: ISLAMIC_GREEN,
        }
    },
    # Kiwi: light green flesh, pale core, black seeds
    0x1F95D: {
        "pixels": "smooth",
        "classes": {
            LIMERICK: INCHWORM,
            BRASS: INCHWORM,
            ARMY_GREEN: BLACK,
        },
    },
    # Banana: pale flesh, yellow peel, darker yellow underside
    0x1F34C: {
        "classes": {
            CHROME_YELLOW: ICTERINE,
            RAJAH: ICTERINE,
            PASTEL_YELLOW: PASTEL_YELLOW,
        },
        "fill": {(15, 24): ICTERINE, (29, 25): ICTERINE, (21, 27): CHROME_YELLOW},
    },
    # Exploding head: white eyes
    0x1F92F: {
        "paint": [(15, 25, 16, 26, WHITE, "fill"), (16, 27, 20, 27, WHITE, "fill")]
    },
    # Sandwich: green lettuce
    0x1F96A: {"classes": {LIMERICK: INCHWORM}},
    # Monkeys: dark brown fur, light brown face
    **{cp: {"nested": [WINDSOR_TAN, RAJAH, RAJAH]} for cp in (0x1F648, 0x1F64A)},
    # the face outline is open at the hairline, so paint the face
    0x1F649: {
        "area": WINDSOR_TAN,
        "black": True,
        "fill": {(12, 29): RAJAH, (33, 29): RAJAH},
        "paint": [(17, 16, 29, 30, RAJAH, WINDSOR_TAN)],
    },
    # Raised hands and weary cat: the arms run off the bottom, close them
    0x1F64C: {"close": [(11, 35, 16, 35), (31, 35, 35, 35)]},
    0x1F640: {"close": [(15, 35, 17, 35), (28, 35, 30, 35)]},
    # Lock: nothing inside the shackle
    0x1F512: {"fill": {(22, 15): None}},
    # Target: the 1-bit rings are thin lines, so the inner red band and the
    # bullseye are drawn
    0x1F3AF: {
        "nested": [RED, WHITE],
        "draw": [
            ("ring", 22.5, 22.5, 0, 3.6, WHITE),
            ("ring", 22.5, 22.5, 3.6, 6.4, RED),
            ("ring", 22.5, 22.5, 0, 1.6, RED),
            ("line", 23, 22, 30, 17, 1, BLACK),
        ],
    },
    # Muted bell: the 1-bit slash stops at the bell, so it's drawn across
    0x1F515: {
        "classes": {DARK_GRAY: WHITE},
        "blobs": CHROME_YELLOW,
        "fill": {(17, 23): CHROME_YELLOW},
        "draw": [
            ("ring", 22.5, 22.5, 10.6, 14, RED),
            ("line", 13, 14, 32, 33, 1.6, RED),
        ],
    },
    # Axe: the end of the handle shows above the head
    0x1FA93: {"fill": {(20, 11): PASTEL_YELLOW}},
    # Game controller: four colored buttons, no outline
    0x1F3AE: {
        "paint": [
            (28, 18, 29, 18, ICTERINE, "all"),
            (26, 19, 27, 20, PICTON_BLUE, "all"),
            (30, 19, 31, 20, RED, "all"),
            (28, 21, 29, 21, ISLAMIC_GREEN, "all"),
        ]
    },
    # Chequered flag: stays black and white
    0x1F3C1: {"colors": [WHITE]},
    # Bee: yellow and light blue in the white areas, last segment yellow
    0x1F41D: {
        "colors": [ICTERINE, CELESTE],
        "black": True,
        "paint": [(31, 23, 35, 29, ICTERINE, "fill")],
    },
    # Turtle: no white on the shell
    0x1F422: {"paint": [(29, 23, 30, 23, BLACK, "all")]},
    # Pig: magenta snout, black nostrils, the mouth only the 1-bit line
    # and black below the ears
    0x1F437: {
        "paint": [
            (11, 17, 14, 19, BLACK, "line"),
            (31, 17, 35, 19, BLACK, "line"),
            (18, 23, 27, 28, BRILLIANT_ROSE, "line"),
            (20, 30, 25, 30, MELON, "unlined"),
        ]
    },
    # Panda: white eyes around the 1-bit pupils
    0x1F43C: {
        "paint": [
            (16, 21, 20, 24, WHITE, "unlined"),
            (27, 21, 31, 24, WHITE, "unlined"),
        ]
    },
    # Tangerine: close the top across the stem, leave above it empty
    0x1F34A: {
        "paint": [
            (24, 15, 25, 15, BLACK, "all"),
            (20, 13, 30, 14, None, CHROME_YELLOW),
            (21, 14, 21, 14, None, "all"),
            (29, 13, 29, 14, None, "all"),
            (20, 9, 33, 18, ISLAMIC_GREEN, WINDSOR_TAN),
        ]
    },
    # Penguin: brown, orange and light yellow bands, white eye
    0x1F427: {
        "paint": [
            (16, 14, 28, 16, WINDSOR_TAN, "fill"),
            (16, 17, 28, 19, CHROME_YELLOW, "fill"),
            (16, 20, 28, 22, PASTEL_YELLOW, "fill"),
            (22, 12, 23, 13, WHITE, "all"),
        ]
    },
    # Dragon: green face, beige snout, red tongue, yellow mane pockets
    0x1F432: {
        "mono": True,
        "pockets": (ICTERINE, [((10, 24, 20, 28), RED)]),
        "flood": [
            (11, 21, PASTEL_YELLOW, (10, 19, 16, 23)),
            (22, 19, INCHWORM, [(10, 11, 18, 23), (19, 17, 25, 23)]),
            (20, 27, INCHWORM, (12, 25, 26, 29)),
            (20, 24, INCHWORM, (19, 24, 25, 24)),
            (14, 30, INCHWORM, (12, 30, 16, 30)),
        ],
        "paint": [(24, 12, 39, 28, DARK_GREEN, BLACK)],
    },
    # Cow: plain black eyes, no white in the nostril; ears pink inside,
    # the left one brown outside like the patch
    0x1F42E: {
        "colors": "all",
        "fill": {(11, 14): WINDSOR_TAN, (16, 15): WINDSOR_TAN, (13, 17): WINDSOR_TAN},
        "paint": [
            (13, 15, 15, 16, MELON, "line"),
            (19, 19, 20, 21, BLACK, "all"),
            (18, 19, 18, 21, WINDSOR_TAN, "all"),
            (21, 19, 21, 21, WINDSOR_TAN, "all"),
            (26, 28, 26, 28, BLACK, "all"),
        ],
    },
    # Frog: green, with a red mouth
    0x1F438: {
        "black": True,
        "area": INCHWORM,
        "paint": [
            (17, 25, 31, 27, ORANGE, INCHWORM),
            (20, 28, 26, 28, ORANGE, INCHWORM),
        ],
    },
    # Rabbit head: gray and white, pink inside the ears
    0x1F430: {
        "colors": [LIGHT_GRAY, WHITE],
        "pixels": "smooth",
        "paint": [(17, 13, 19, 20, MELON, "line"), (26, 13, 28, 20, MELON, "line")],
    },
    # Monkey: colored pixel by pixel, red mouth
    0x1F435: {
        "pixels": True,
        "paint": [(19, 27, 28, 29, RED, "all")],
    },
    # Briefcase: buckle lighter than the case
    0x1F4BC: {"fill": {(21, 23): RAJAH, (21, 26): RAJAH}},
    # Rabbit: gray with a white tail
    0x1F407: {"area": LIGHT_GRAY, "paint": [(30, 26, 32, 30, WHITE, "fill")]},
    # Paperclips: gray wire, holes in the loops left empty
    0x1F587: {"nested": [LIGHT_GRAY, None], "paint": [(18, 24, 20, 25, None, "fill")]},
    # Avocado: light green flesh, brown pit, dark green skin
    0x1F951: {
        "paint": [
            (0, 0, 39, 39, ISLAMIC_GREEN, INCHWORM),
            (0, 0, 39, 39, INCHWORM, RAJAH),
            (25, 21, 39, 39, ISLAMIC_GREEN, WINDSOR_TAN),
            (0, 27, 39, 39, ISLAMIC_GREEN, WINDSOR_TAN),
        ]
    },
    # Lemon: yellow, orange shading where the art has it, two-tone green leaf
    0x1F34B: {
        "paint": [
            (0, 0, 39, 39, ICTERINE, CHROME_YELLOW),
            (10, 19, 12, 28, ISLAMIC_GREEN, "fill"),
            (13, 19, 14, 28, INCHWORM, "fill"),
            (15, 24, 15, 28, INCHWORM, WINDSOR_TAN),
        ],
        "artbelow": [((16, 14, 39, 39), 1, 185, CHROME_YELLOW)],
    },
    # Pancakes: yellow butter on top, brown syrup around it, brown lines
    # between the pancakes where the 1-bit glyph has them
    0x1F95E: {
        "paint": [
            (14, 11, 21, 16, WINDSOR_TAN, RAJAH),
            (20, 12, 26, 16, ICTERINE, WINDSOR_TAN),
            (0, 18, 39, 32, WINDSOR_TAN, "lined"),
        ]
    },
    # Bacon: the 1-bit lines as they are, pink fat along the top and salmon
    # meat below
    0x1F953: {
        "mono": True,
        "flood": [
            (25, 20, MELON, [(31, 13, 33, 15), (25, 15, 32, 15), (0, 16, 39, 39)]),
            *(
                (x, y, SUNSET_ORANGE, (0, 0, 39, 39))
                for x, y in ((28, 19), (35, 14), (10, 29), (15, 29), (13, 30), (12, 31))
            ),
        ],
    },
    # Croissant: the 1-bit lines as they are, filled light brown
    0x1F950: {
        "mono": True,
        "flood": [
            (x, y, RAJAH, (0, 0, 39, 39)) for x, y in ((17, 11), (32, 14), (13, 26))
        ],
    },
    # Dotted line face: orange dots, as at 28 point
    0x1FAE5: {"ink": CHROME_YELLOW},
    # No entry: drawn on the 1-bit circle
    0x1F6AB: {"no_entry": True},
    # Cherries: green leaf only, nothing between the stems; red, a light
    # band at the top left and a dark one at the bottom right
    0x1F352: {
        "paint": [
            (15, 15, 30, 24, None, ISLAMIC_GREEN),
            (13, 24, 34, 35, RED, SUNSET_ORANGE),
            (15, 26, 16, 26, SUNSET_ORANGE, RED),
            (14, 27, 15, 28, SUNSET_ORANGE, RED),
            (14, 29, 14, 29, SUNSET_ORANGE, RED),
            (24, 26, 25, 27, SUNSET_ORANGE, RED),
            (25, 28, 25, 28, SUNSET_ORANGE, RED),
            (20, 32, 22, 32, DARK_CANDY_APPLE_RED, RED),
            (19, 33, 22, 33, DARK_CANDY_APPLE_RED, RED),
            (30, 31, 31, 31, DARK_CANDY_APPLE_RED, RED),
            (29, 32, 30, 32, DARK_CANDY_APPLE_RED, RED),
            (28, 33, 29, 33, DARK_CANDY_APPLE_RED, RED),
        ]
    },
    # Mobile phone: the 1-bit glyph is solid black, so the screen and a grid
    # of app icons are drawn
    0x1F4F1: {
        "paint": [(17, 13, 30, 33, CELESTE, "all")]
        + [
            (x, y, x + 1, y + 1, c, "all")
            for (x, y), c in zip(
                [(x, y) for y in (14, 17, 20, 23) for x in (18, 21, 24, 27)],
                [
                    RED,
                    CHROME_YELLOW,
                    YELLOW,
                    ISLAMIC_GREEN,
                    COBALT_BLUE,
                    BRILLIANT_ROSE,
                    VIVID_VIOLET,
                    TIFFANY_BLUE,
                    ORANGE,
                    KELLY_GREEN,
                    BLUE,
                    JAZZBERRY_JAM,
                    RED,
                    ISLAMIC_GREEN,
                    COBALT_BLUE,
                    CHROME_YELLOW,
                ],
            )
        ]
    },
    # Halo: blue ring, yellow face like other faces
    0x1F607: {
        "paint": [
            (17, 13, 28, 15, ICTERINE, CELESTE),
            (10, 10, 36, 15, PICTON_BLUE, CELESTE),
            (10, 10, 36, 15, PICTON_BLUE, BLACK),
            (10, 10, 36, 12, CELESTE, ICTERINE),
            (10, 13, 16, 15, CELESTE, ICTERINE),
            (29, 13, 36, 15, CELESTE, ICTERINE),
            (19, 12, 26, 12, BLACK, "all"),
            (17, 13, 19, 13, BLACK, "all"),
            (26, 13, 28, 13, BLACK, "all"),
            (15, 14, 16, 15, BLACK, PICTON_BLUE),
            (29, 14, 30, 15, BLACK, PICTON_BLUE),
            (16, 15, 16, 15, ICTERINE, "all"),
            (29, 15, 29, 15, ICTERINE, "all"),
            (13, 16, 32, 16, PICTON_BLUE, BLACK),
            (17, 17, 28, 17, PICTON_BLUE, BLACK),
        ]
    },
    # Police officer: yellow face, black cap with a gold badge in the middle
    0x1F46E: {
        "paint": [
            (25, 17, 31, 21, BLACK, CHROME_YELLOW),
            (15, 19, 33, 35, ICTERINE, CHROME_YELLOW),
            (15, 18, 31, 21, BLACK, ICTERINE),
            (21, 16, 24, 17, CHROME_YELLOW, "all"),
        ]
    },
    # Princess: yellow face like the other people, below the crown's gold
    # bottom row
    0x1F478: {"paint": [(14, 21, 34, 35, ICTERINE, CHROME_YELLOW)]},
}


def _rgb(v):
    """Turn "#RGB" strings into RGB tuples, anywhere in v."""
    if isinstance(v, str) and re.fullmatch(r"#[0-9A-F]{3}", v):
        return tuple(int(c, 16) * 17 for c in v[1:])
    if isinstance(v, dict):
        return {_rgb(k): _rgb(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return type(v)(_rgb(x) for x in v)
    return v


OV = _rgb(OV)
