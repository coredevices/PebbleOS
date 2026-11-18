# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import json
import os
import sys
import tempfile
import unittest

# Allow us to run even if not at the `tools` directory.
root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(0, root_dir)

import pebble_sdk_platform as sdk_platform
from pebble_sdk_platform import PlatformError, pebble_platforms

KCONFIG = os.path.join(os.path.dirname(root_dir), "pebble", "Kconfig.platform")

# A platform no board has.
SLATE = {
    "version": 1,
    "name": "slate",
    "abi": "armv7m",
    "display": {"width": 240, "height": 240, "shape": "rect", "color": True},
    "features": ["speaker", "microphone"],
    "limits": {
        "app_binary": 196608,
        "app_memory": 196608,
        "worker_memory": 10240,
        "resources": 1048576,
        "resources_appstore": 262144,
        "font_glyph": 512,
    },
}


class TestPlatformTable(unittest.TestCase):
    def test_kconfig_platforms_are_known(self):
        for name in ("emery", "flint", "gabbro"):
            self.assertIn(name, pebble_platforms)
        for name in ("aplite", "basalt", "chalk", "diorite"):
            self.assertIn("FROZEN_AT_REVISION", pebble_platforms[name])

    def test_emery(self):
        emery = pebble_platforms["emery"]
        self.assertEqual(
            set(emery["DEFINES"]),
            {
                "PBL_PLATFORM_EMERY",
                "PBL_COLOR",
                "PBL_RECT",
                "PBL_MICROPHONE",
                "PBL_SMARTSTRAP",
                "PBL_SMARTSTRAP_POWER",
                "PBL_HEALTH",
                "PBL_COMPASS",
                "PBL_TOUCH",
                "PBL_RGB_BACKLIGHT",
                "PBL_SPEAKER",
                "PBL_DISPLAY_WIDTH=200",
                "PBL_DISPLAY_HEIGHT=228",
            },
        )
        self.assertEqual(emery["MAX_APP_MEMORY_SIZE"], 0x20000)
        self.assertEqual(emery["MAX_FONT_GLYPH_SIZE"], 512)
        self.assertTrue(emery["HAS_MODDABLE_XS"])

    def test_flint(self):
        flint = pebble_platforms["flint"]
        self.assertEqual(
            set(flint["TAGS"]),
            {
                "flint",
                "bw",
                "rect",
                "mic",
                "health",
                "compass",
                "speaker",
                "144w",
                "168h",
            },
        )
        self.assertEqual(flint["MAX_APP_MEMORY_SIZE"], 0x10000)
        self.assertEqual(flint["MAX_FONT_GLYPH_SIZE"], 256)
        self.assertNotIn("HAS_MODDABLE_XS", flint)

    def test_frozen_platform(self):
        aplite = pebble_platforms["aplite"]
        self.assertIn("PBL_SDK_FROZEN", aplite["DEFINES"])
        self.assertEqual(aplite["FROZEN_AT_REVISION"], 81)
        self.assertEqual(aplite["MAX_APP_MEMORY_SIZE"], 0x6000)
        self.assertEqual(aplite["MAX_RESOURCES_SIZE_APPSTORE_2_X"], 0x18000)


class TestDescriptor(unittest.TestCase):
    def test_bundle_description(self):
        self.assertEqual(
            sdk_platform.bundle_description("flint"),
            {
                "version": 1,
                "name": "flint",
                "abi": "armv7m",
                "display": {
                    "width": 144,
                    "height": 168,
                    "shape": "rect",
                    "color": False,
                },
                "features": sdk_platform.pebble_platforms["flint"]["DESCRIPTOR"][
                    "features"
                ],
                "limits": {"app_memory": 65536},
            },
        )
        # Frozen platforms are described too.
        self.assertEqual(sdk_platform.bundle_description("chalk")["name"], "chalk")

    def test_new_platform(self):
        slate = sdk_platform.platform_from_descriptor(SLATE)
        # In the table's order, not the description's.
        self.assertEqual(
            slate["DEFINES"],
            [
                "PBL_PLATFORM_SLATE",
                "PBL_COLOR",
                "PBL_RECT",
                "PBL_MICROPHONE",
                "PBL_SPEAKER",
                "PBL_DISPLAY_WIDTH=240",
                "PBL_DISPLAY_HEIGHT=240",
            ],
        )
        self.assertEqual(
            slate["TAGS"], ["slate", "color", "rect", "mic", "speaker", "240w", "240h"]
        )
        self.assertEqual(slate["MAX_APP_MEMORY_SIZE"], 196608)
        self.assertEqual(slate["BUNDLE_BIN_DIR"], "slate")
        self.assertNotIn("FROZEN_AT_REVISION", slate)

    def _invalid(self, **changes):
        desc = json.loads(json.dumps(SLATE))
        for key, value in changes.items():
            if value is None:
                del desc[key]
            else:
                desc[key] = value
        with self.assertRaises(PlatformError):
            sdk_platform.validate_descriptor(desc)

    def test_invalid(self):
        self._invalid(name="Slate")
        self._invalid(abi="riscv")
        self._invalid(display=None)
        self._invalid(display=dict(SLATE["display"], shape="square"))
        self._invalid(display=dict(SLATE["display"], width=0))
        self._invalid(display=dict(SLATE["display"], color=1))
        self._invalid(features=["laser"])
        self._invalid(limits={"app_memory": 65536})
        self._invalid(limits=dict(SLATE["limits"], heap=1))

    def test_from_kconfig(self):
        kconf = sdk_platform.kconfig_platform(KCONFIG, "PLATFORM_GABBRO")
        desc = sdk_platform.descriptor_from_kconfig(sdk_platform.kconfig_symbols(kconf))
        self.assertEqual(desc["name"], "gabbro")
        self.assertEqual(desc["abi"], "armv7m")
        self.assertEqual(
            desc["display"],
            {"width": 260, "height": 260, "shape": "round", "color": True},
        )
        self.assertEqual(desc["features"], ["microphone", "health", "compass", "touch"])
        self.assertTrue(desc["moddable"])

    def test_not_a_platform(self):
        with self.assertRaises(PlatformError):
            sdk_platform.kconfig_platform(KCONFIG, "PLATFORM_COLOR")

    def test_sdk_descriptors(self):
        with tempfile.TemporaryDirectory() as sdk:
            os.makedirs(os.path.join(sdk, "slate"))
            os.makedirs(os.path.join(sdk, "common"))
            with open(os.path.join(sdk, "slate", "platform.json"), "w") as f:
                json.dump(SLATE, f)
            self.assertEqual(sdk_platform.sdk_descriptors(sdk), [SLATE])

            with open(os.path.join(sdk, "slate", "platform.json"), "w") as f:
                json.dump(dict(SLATE, version=2), f)
            with self.assertRaises(PlatformError):
                sdk_platform.sdk_descriptors(sdk)


if __name__ == "__main__":
    unittest.main()
