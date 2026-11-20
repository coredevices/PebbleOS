# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "sdk" / "tools"))

import pebble_build


class TestPackageBuild(unittest.TestCase):
    def _lib(self, tmp, builds, media=None, resos=()):
        for build in builds:
            os.makedirs(os.path.join(tmp, "include", "lib", build))
            os.makedirs(os.path.join(tmp, "resources", build))
        for build, name in resos:
            Path(tmp, "resources", build, f"{name.lower()}.png.{name}.reso").touch()
        lib = {"name": "lib", "path": tmp}
        if media:
            lib["pebble"] = {"resources": {"media": media}}
        return lib

    def test_a_build_takes_the_package_build_of_its_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            lib = self._lib(tmp, ["basalt", "rect-color", "rect-color+mic"])
            self.assertEqual(pebble_build._package_build(lib, "basalt"), "basalt")
            self.assertEqual(
                pebble_build._package_build(lib, "rect-color+mic"), "rect-color+mic"
            )
            self.assertEqual(
                pebble_build._package_build(lib, "rect-color"), "rect-color"
            )

    def test_a_platform_takes_no_other_build(self):
        with tempfile.TemporaryDirectory() as tmp:
            lib = self._lib(tmp, ["rect-color", "basalt"])
            self.assertIsNone(pebble_build._package_build(lib, "emery"))
            self.assertIsNone(pebble_build._package_build(lib, "rect-bw"))

    def test_a_flexible_build_takes_the_most_of_its_features(self):
        with tempfile.TemporaryDirectory() as tmp:
            lib = self._lib(tmp, ["rect-color", "rect-color+mic", "rect-color+compass"])
            build = pebble_build._package_build(lib, "rect-color+mic+health")
            self.assertEqual(build, "rect-color+mic")
            # Never one that requires what the binary does not.
            self.assertEqual(
                pebble_build._package_build(lib, "rect-color+health"), "rect-color"
            )

    def test_resources_are_those_of_the_package_build(self):
        media = [
            {"name": "ICON", "file": "icon.png", "type": "png"},
            {
                "name": "MIC",
                "file": "mic.png",
                "type": "png",
                "targetPlatforms": ["mic"],
            },
        ]
        with tempfile.TemporaryDirectory() as tmp:
            lib = self._lib(tmp, ["rect-color"], media, [("rect-color", "ICON")])
            resos = pebble_build._package_resources(
                {"libraries": [lib]}, "rect-color+mic"
            )
            self.assertEqual(list(resos), ["ICON"])


class TestChosenPack(unittest.TestCase):
    def test_largest_width_then_height_that_fits(self):
        packs = [((0, 168), "h"), ((200, 0), "w"), ((200, 228), "wh")]
        self.assertEqual(pebble_build._chosen_pack(packs, (144, 168), "d"), "h")
        self.assertEqual(pebble_build._chosen_pack(packs, (200, 180), "d"), "w")
        self.assertEqual(pebble_build._chosen_pack(packs, (260, 260), "d"), "wh")

    def test_default_when_none_fits(self):
        self.assertEqual(
            pebble_build._chosen_pack([((200, 0), "w")], (144, 168), "d"), "d"
        )
        self.assertEqual(pebble_build._chosen_pack([], (144, 168), "d"), "d")


if __name__ == "__main__":
    unittest.main()
