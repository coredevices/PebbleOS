# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))


from pbpack import ResourcePack
from resources.find_resource_filename import find_most_specific_filename, size_of_tags

from resources import generators


class Env(dict):
    """A build's settings, as the builds hand them over: one that is not set
    reads back as an empty list."""

    def __getattr__(self, key):
        return self.get(key, [])


class _Bld:
    def fatal(self, msg):
        raise RuntimeError(msg)


class _Root:
    def __init__(self, path):
        self.path = path

    def relpath(self):
        return self.path


class TestFindResourceFilename(unittest.TestCase):
    FILES = (
        "icon.png",
        "icon~bw.png",
        "icon~144w.png",
        "icon~200w.png",
        "icon~228h.png",
        "icon~260w~260h.png",
        "icon~color~200w.png",
    )

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        for name in self.FILES:
            Path(self.tmp.name, name).touch()

    def tearDown(self):
        self.tmp.cleanup()

    def find(self, platform, breakpoint_size=None, name="icon.png"):
        env = Env(PLATFORM_NAME=platform)
        if breakpoint_size is not None:
            env.RESOURCE_BREAKPOINT = breakpoint_size
        return find_most_specific_filename(_Bld(), env, _Root(self.tmp.name), name)

    def test_size_of_tags(self):
        self.assertEqual(size_of_tags(["200w", "color"]), ((200, 0), {"color"}))
        self.assertEqual(size_of_tags(["260w", "260h"]), ((260, 260), set()))
        self.assertEqual(size_of_tags([]), ((0, 0), set()))

    def test_a_platform_takes_the_file_for_its_very_display(self):
        # As before: a size tag is one of the platform's tags.
        self.assertEqual(self.find("gabbro"), "icon~260w~260h.png")
        self.assertEqual(self.find("basalt"), "icon~144w.png")
        os.remove(os.path.join(self.tmp.name, "icon~144w.png"))
        self.assertEqual(self.find("basalt"), "icon.png")
        self.assertEqual(self.find("diorite"), "icon~bw.png")

    def test_a_breakpoint_takes_the_largest_that_is_no_larger(self):
        self.assertEqual(self.find("rect-bw", (0, 0)), "icon~bw.png")
        os.remove(os.path.join(self.tmp.name, "icon~color~200w.png"))
        self.assertEqual(self.find("round-color", (0, 0)), "icon.png")
        self.assertEqual(self.find("round-color", (144, 0)), "icon~144w.png")
        self.assertEqual(self.find("round-color", (199, 500)), "icon~144w.png")
        self.assertEqual(self.find("round-color", (200, 0)), "icon~200w.png")
        self.assertEqual(self.find("round-color", (260, 259)), "icon~200w.png")
        self.assertEqual(self.find("round-color", (260, 260)), "icon~260w~260h.png")
        self.assertEqual(self.find("round-color", (0, 228)), "icon~228h.png")

    def test_a_tag_of_the_build_counts_before_the_size(self):
        self.assertEqual(self.find("rect-color", (200, 228)), "icon~color~200w.png")
        self.assertEqual(self.find("rect-color", (144, 168)), "icon~144w.png")
        # Black and white has a file of its own, whatever the display.
        self.assertEqual(self.find("rect-bw", (400, 400)), "icon~bw.png")

    def test_a_platform_in_a_flexible_project_goes_by_breakpoints_too(self):
        self.assertEqual(self.find("emery", (200, 228)), "icon~color~200w.png")
        self.assertEqual(self.find("flint", (144, 168)), "icon~bw.png")
        # Where both of its size tags would have matched a file each.
        os.remove(os.path.join(self.tmp.name, "icon~color~200w.png"))
        self.assertEqual(self.find("emery", (200, 228)), "icon~200w.png")
        with self.assertRaises(RuntimeError):
            self.find("emery")


class TestResourceSet(unittest.TestCase):
    def test_set_id_changes_the_manifest_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "app_resources.pbpack")
            pack = ResourcePack(is_system=False)
            pack.add_resource(b"one")
            pack.add_resource(b"another")
            with open(path, "wb") as f:
                pack.serialize(f)
            with open(path, "rb") as f:
                before = f.read()

            generators.set_pbpack_set_id(path, 0x12345678)
            with open(path, "rb") as f:
                after = f.read()
                f.seek(0)
                marked = ResourcePack.deserialize(f, is_system=False)

        self.assertEqual(marked.timestamp, 0x12345678)
        self.assertEqual(marked.crc, pack.crc)
        self.assertEqual(len(after), len(before))
        self.assertEqual(
            after[ResourcePack.MANIFEST_SIZE_BYTES :],
            before[ResourcePack.MANIFEST_SIZE_BYTES :],
        )


if __name__ == "__main__":
    unittest.main()
