# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import generate_appinfo
import pebble_sdk_platform as sdk_platform

APP_INFO = {
    "uuid": "1f0b0701-cc8f-47ec-86e7-7181397f9a25",
    "shortName": "app",
    "companyName": "company",
    "versionLabel": "1.0",
}

# A platform PebbleProcessInfoFlags has no value for.
SLATE = {
    "version": 1,
    "name": "slate",
    "abi": "armv7m",
    "display": {"width": 240, "height": 200, "shape": "round", "color": True},
    "features": ["speaker", "microphone"],
    "limits": {
        "app_binary": 196608,
        "app_memory": 100000,
        "worker_memory": 10240,
        "resources": 1048576,
        "resources_appstore": 262144,
        "font_glyph": 512,
    },
}


class TestGenerateAppinfo(unittest.TestCase):
    def generate(self, platform_name):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "appinfo.auto.c")
            generate_appinfo.generate_appinfo_c(APP_INFO, path, platform_name)
            with open(path) as f:
                return f.read()

    def test_platform_info_of_a_description(self):
        self.assertEqual(
            generate_appinfo.platform_info_initializer(SLATE),
            "{ PROCESS_PLATFORM_INFO_VERSION, sizeof(PebblePlatformInfo), 1, 1, "
            "240, 200, 0x81, 98, 6, 0 }",
        )

    def test_platform_with_a_flag_value(self):
        source = self.generate("emery")
        self.assertIn(
            "PROCESS_INFO_PLATFORM_EMERY | PROCESS_INFO_HAS_PLATFORM_INFO", source
        )
        self.assertIn('__section__(".pbl_platform")', source)
        self.assertIn(", 200, 228, ", source)

    def test_platform_without_a_flag_value(self):
        platforms = dict(
            sdk_platform.pebble_platforms,
            slate=sdk_platform.platform_from_descriptor(SLATE),
        )
        with patch.object(sdk_platform, "pebble_platforms", platforms):
            source = self.generate("slate")
        self.assertIn(
            "PROCESS_INFO_PLATFORM_OTHER | PROCESS_INFO_HAS_PLATFORM_INFO", source
        )
        self.assertIn(", 240, 200, ", source)

    def test_frozen_platform_has_no_platform_info(self):
        source = self.generate("basalt")
        self.assertIn("PROCESS_INFO_PLATFORM_BASALT", source)
        self.assertNotIn("PLATFORM_INFO", source)
        self.assertNotIn(".pbl_platform", source)

    def test_flexible_build(self):
        source = self.generate("rect-color+mic")
        self.assertIn(
            "PROCESS_INFO_PLATFORM_OTHER | PROCESS_INFO_HAS_PLATFORM_INFO", source
        )
        # No display size, the feature it requires, 64K
        self.assertIn(
            "1, 0, 0, 0, 0x1, 64, 6, PROCESS_PLATFORM_INFO_FLAG_FLEXIBLE }", source
        )

    def test_flexible_build_that_asks_for_memory(self):
        with patch.dict(APP_INFO, appMemory=131072):
            self.assertIn(", 0x0, 128, 1, ", self.generate("round-bw"))
            # A platform gives what it gives
            self.assertIn(", 128, 6, 0 }", self.generate("emery"))

    def test_no_platform(self):
        source = self.generate(None)
        self.assertIn(".flags = 0,", source)
        self.assertNotIn(".pbl_platform", source)


if __name__ == "__main__":
    unittest.main()
