# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import contextlib
import io
import os
import sys
import unittest

# Allow us to run even if not at the `tools` directory.
root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(0, root_dir)

from resources.resource_map.resource_generator_font import FontResourceGenerator
from resources.types.resource_definition import ResourceDefinition

height_from_name = FontResourceGenerator._get_font_height_from_name

TTF_PATH = os.path.join(
    root_dir, os.pardir, "resources/normal/base/ttf/Roboto-Condensed.ttf"
)


def warning_for(name):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        FontResourceGenerator._warn_if_height_moved(name, height_from_name(name))
    return out.getvalue()


def font_definition(name, pixel_height=None, extended=False, regex="[0]"):
    definition = ResourceDefinition("font", name, TTF_PATH)
    definition.max_glyph_size = 256
    definition.character_list = None
    definition.character_regex = regex
    definition.compatibility = None
    definition.compress = None
    definition.extended = extended
    definition.tracking_adjust = None
    definition.pixel_height = pixel_height
    return definition


def build_font(name, pixel_height=None, extended=False, regex="[0]"):
    """Build a few glyphs of the font, returning the stored height and anything printed."""
    definition = font_definition(name, pixel_height, extended, regex)
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        data = FontResourceGenerator.build_font_data(TTF_PATH, definition)
    # The header opens with the version byte, then max_height
    return data[1], out.getvalue()


class TestFontHeightFromName(unittest.TestCase):
    def test_system_font_names(self):
        # Every name the firmware and the language packs build keeps its height
        cases = {
            "GOTHIC_09": 9,
            "GOTHIC_18_BOLD": 18,
            "GOTHIC_24_BOLD_EXTENDED": 24,
            "LECO_60_NUMBERS_AM_PM": 60,
            "ROBOTO_BOLD_SUBSET_49": 49,
            "ROBOTO_CONDENSED_21": 21,
            "MINCHO_24_PAIR": 24,
            "DROID_SERIF_28_BOLD": 28,
            "FONT_ROBOTO_BOLD_SUBSET_49": 49,
        }
        for name, height in cases.items():
            with self.subTest(name=name):
                self.assertEqual(height_from_name(name), height)

    def test_documented_app_names(self):
        self.assertEqual(height_from_name("EXAMPLE_FONT_20"), 20)
        self.assertEqual(height_from_name("FONT_OSWALD_24"), 24)

    def test_number_inside_a_word_is_skipped(self):
        # A digit inside a word is part of the name, not the size
        for name in ("FONT_PIXEL8BIT_24", "FONT_RETRO0_24", "FONT_V2_ROBOTO_24"):
            with self.subTest(name=name):
                self.assertEqual(height_from_name(name), 24)

    def test_size_at_the_end_wins_over_an_earlier_number(self):
        # The guide says the name ends with the size
        self.assertEqual(height_from_name("FONT_2_ROBOTO_24"), 24)
        self.assertEqual(height_from_name("RETRO_2_FONT_20"), 20)
        self.assertEqual(height_from_name("FONT_2024_18"), 18)

    def test_number_only_inside_a_word_still_counts(self):
        # With no standalone number, the first number anywhere is the size
        self.assertEqual(height_from_name("FONT_OSWALD24"), 24)
        self.assertEqual(height_from_name("OSWALD24"), 24)

    def test_name_without_a_number(self):
        with self.assertRaisesRegex(ValueError, "no height found in name"):
            height_from_name("FONT_OSWALD")
        self.assertEqual(height_from_name("FONT_FALLBACK"), 14)
        self.assertEqual(height_from_name("FONT_FALLBACK_INTERNAL"), 14)


class TestHeightMovedWarning(unittest.TestCase):
    def test_warns_when_the_height_differs_from_older_sdks(self):
        for name, old in (("FONT_PIXEL8BIT_24", "8"), ("FONT_2_ROBOTO_24", "2")):
            with self.subTest(name=name):
                out = warning_for(name)
                self.assertIn(name, out)
                self.assertIn("the name gives 24", out)
                self.assertIn(f"Older SDKs read {old}", out)

    def test_quiet_when_the_height_is_unchanged(self):
        for name in ("GOTHIC_18_BOLD", "EXAMPLE_FONT_20", "FONT_OSWALD24"):
            with self.subTest(name=name):
                self.assertEqual(warning_for(name), "")


class TestBuildFontData(unittest.TestCase):
    def test_stores_the_name_height_and_warns(self):
        height, out = build_font("FONT_PIXEL8BIT_24")
        self.assertEqual(height, 24)
        self.assertIn("Older SDKs read 8", out)

    def test_pixel_height_skips_the_warning(self):
        height, out = build_font("FONT_PIXEL8BIT_24", pixel_height=24)
        self.assertEqual(height, 24)
        self.assertEqual(out, "")

    def test_pixel_height_needs_no_number_in_the_name(self):
        height, _ = build_font("FONT_MYFONT", pixel_height=20)
        self.assertEqual(height, 20)

    def test_pixel_height_given_as_a_string(self):
        # A string from package.json goes through int()
        height, _ = build_font("FONT_MYFONT", pixel_height="20")
        self.assertEqual(height, 20)
        with self.assertRaisesRegex(
            ValueError, "pixelHeight 'big' is not a whole number"
        ):
            build_font("FONT_MYFONT", pixel_height="big")

    def test_extended_font_still_takes_its_baseline_from_the_name(self):
        with self.assertRaisesRegex(ValueError, "no height found in name"):
            build_font("FONT_MYFONT", pixel_height=20, extended=True)
        with self.assertRaisesRegex(ValueError, "the name gives 0"):
            build_font("GOTHIC_0_EXTENDED", pixel_height=14, extended=True)

    def test_extended_font_with_pixel_height_still_warns_about_the_name(self):
        # pixelHeight sets the height, and the name still moves the baseline
        height, out = build_font(
            "FONT_PIXEL8BIT_24_EXTENDED", pixel_height=20, extended=True
        )
        self.assertEqual(height, 20)
        self.assertIn("the name gives 24", out)

    def test_pixel_height_refuses_a_boolean(self):
        # int() would turn true into a 1 pixel font
        for pixel_height in (True, False):
            with (
                self.subTest(pixel_height=pixel_height),
                self.assertRaisesRegex(ValueError, "is not a whole number"),
            ):
                build_font("FONT_MYFONT", pixel_height=pixel_height)

    def test_height_range_edges(self):
        # A real glyph at 255 pixels overflows fontgen, so the top edge skips the build
        height, _ = build_font("FONT_EDGE", 1, regex="[ ]")
        self.assertEqual(height, 1)
        definition = font_definition("FONT_EDGE", 255)
        self.assertEqual(FontResourceGenerator._get_pixel_height(definition), 255)
        for pixel_height in (0, 256):
            with (
                self.subTest(pixel_height=pixel_height),
                self.assertRaisesRegex(
                    ValueError, f"pixelHeight {pixel_height} is outside 1 to 255"
                ),
            ):
                build_font("FONT_EDGE", pixel_height, regex="[ ]")

    def test_name_height_out_of_range_is_refused(self):
        for name, height in (("FONT_RETRO0", 0), ("FONT_X_300", 300)):
            with (
                self.subTest(name=name),
                self.assertRaisesRegex(ValueError, f"the name gives {height}"),
            ):
                build_font(name, regex="[ ]")

    def test_size_at_the_end_of_a_name_with_a_year(self):
        height, out = build_font("FONT_2024_18")
        self.assertEqual(height, 18)
        self.assertIn("Older SDKs read 2024", out)

    def test_documented_pixel_height_example(self):
        # The docs' example builds at its size with or without pixelHeight
        self.assertEqual(build_font("RETRO_2_FONT_20", pixel_height=20), (20, ""))
        height, out = build_font("RETRO_2_FONT_20")
        self.assertEqual(height, 20)
        self.assertIn("Older SDKs read 2", out)


if __name__ == "__main__":
    unittest.main()
