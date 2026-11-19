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
                "PBL_MODDABLE",
            },
        )
        self.assertEqual(emery["ABI"], "armv7m")
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
        # Directories an SDK has beside its platforms.
        self._invalid(name="armv7m")
        self._invalid(name="common")
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

    def test_sdk_component(self):
        with tempfile.TemporaryDirectory() as sdk:
            # Flint has nothing of its own, Emery has its own headers.
            for d in ("armv7m/include", "armv7m/lib", "emery/include", "flint"):
                os.makedirs(os.path.join(sdk, d))
            self.assertEqual(
                sdk_platform.sdk_component(sdk, "flint", "include"),
                os.path.join(sdk, "armv7m", "include"),
            )
            self.assertEqual(
                sdk_platform.sdk_component(sdk, "emery", "include"),
                os.path.join(sdk, "emery", "include"),
            )
            self.assertEqual(
                sdk_platform.sdk_component(sdk, "emery", "lib"),
                os.path.join(sdk, "armv7m", "lib"),
            )
            self.assertIsNone(sdk_platform.sdk_component(sdk, "flint", "qemu"))

    def test_sdk_component_frozen(self):
        with tempfile.TemporaryDirectory() as sdk:
            # A frozen platform never takes what the ABI directory has.
            for d in ("armv7m/include", "armv7m/lib", "aplite/include"):
                os.makedirs(os.path.join(sdk, d))
            self.assertEqual(
                sdk_platform.sdk_component(sdk, "aplite", "include"),
                os.path.join(sdk, "aplite", "include"),
            )
            self.assertIsNone(sdk_platform.sdk_component(sdk, "aplite", "lib"))
            self.assertIsNone(sdk_platform.sdk_component(sdk, "basalt", "include"))


class TestFlexible(unittest.TestCase):
    def test_class_of_a_platform(self):
        def of(name):
            return sdk_platform.display_class(
                pebble_platforms[name]["DESCRIPTOR"]["display"]
            )

        self.assertEqual(of("emery"), "rect-color")
        self.assertEqual(of("basalt"), "rect-color")
        self.assertEqual(of("flint"), "rect-bw")
        self.assertEqual(of("gabbro"), "round-color")
        # No platform is named like a class.
        with self.assertRaises(PlatformError):
            sdk_platform.validate_descriptor(dict(SLATE, name="rect-color"))

    def test_built_without_a_size_or_a_name(self):
        build = sdk_platform.flexible_build("rect", True)
        self.assertEqual(build["NAME"], "rect-color")
        self.assertEqual(build["DEFINES"], ["PBL_FLEXIBLE", "PBL_COLOR", "PBL_RECT"])
        self.assertEqual(build["TAGS"], ["color", "rect"])
        self.assertEqual(build["REQUIRES"], [])

    def test_required_features_are_given(self):
        build = sdk_platform.flexible_build(
            "round", False, requires=["speaker", "microphone"]
        )
        self.assertEqual(build["NAME"], "round-bw+mic+speaker")
        self.assertEqual(
            build["DEFINES"],
            ["PBL_FLEXIBLE", "PBL_BW", "PBL_ROUND", "PBL_MICROPHONE", "PBL_SPEAKER"],
        )
        self.assertEqual(build["TAGS"], ["bw", "round", "mic", "speaker"])
        self.assertEqual(build["REQUIRES"], ["microphone", "speaker"])
        with self.assertRaises(PlatformError):
            sdk_platform.flexible_build("rect", True, requires=["laser"])
        # A feature without a resource tag has no name to go by.
        with self.assertRaises(PlatformError):
            sdk_platform.flexible_build("rect", True, requires=["rgb_backlight"])

    def test_names(self):
        parse = sdk_platform.parse_flexible_name
        self.assertEqual(parse("rect-bw"), ("rect", False, []))
        self.assertEqual(
            parse("round-color+mic+health"), ("round", True, ["microphone", "health"])
        )
        # One name for one build: features in their own order, each once.
        self.assertIsNone(parse("round-color+health+mic"))
        self.assertIsNone(parse("rect-color+mic+mic"))
        self.assertIsNone(parse("rect-color+laser"))
        self.assertIsNone(parse("rect-color+"))
        self.assertIsNone(parse("rect-color~mic"))
        self.assertIsNone(parse("emery"))
        self.assertIsNone(parse("rect"))

    def test_looked_up_like_a_platform(self):
        build = pebble_platforms["rect-color+mic"]
        self.assertEqual(
            build, sdk_platform.flexible_build("rect", True, ["microphone"])
        )
        # But it is not one of the platforms.
        self.assertNotIn("rect-color+mic", pebble_platforms)
        self.assertNotIn("rect-color+mic", list(pebble_platforms))
        with self.assertRaises(KeyError):
            pebble_platforms["rect-color+laser"]
        with self.assertRaises(KeyError):
            pebble_platforms["slate"]

    def test_limits(self):
        # 64K whatever the platforms of the class give, and otherwise no more
        # than any of them does.
        build = pebble_platforms["rect-color"]
        self.assertEqual(build["MAX_APP_MEMORY_SIZE"], 0x10000)
        self.assertEqual(build["MAX_APP_BINARY_SIZE"], 0x10000)
        emery = pebble_platforms["emery"]
        for key in (
            "MAX_RESOURCES_SIZE",
            "MAX_WORKER_MEMORY_SIZE",
            "MAX_FONT_GLYPH_SIZE",
        ):
            self.assertLessEqual(build[key], emery[key])
        # A class no platform of the SDK is of still builds.
        self.assertEqual(pebble_platforms["round-bw"]["MAX_FONT_GLYPH_SIZE"], 256)

    def test_what_a_pbw_says(self):
        self.assertEqual(
            sdk_platform.bundle_description("rect-color+mic"),
            {
                "version": 1,
                "name": "rect-color+mic",
                "abi": "armv7m",
                "flexible": True,
                "display": {"shape": "rect", "color": True},
                "requires": ["microphone"],
                "limits": {"app_memory": 0x10000},
            },
        )
        description = sdk_platform.bundle_description("rect-bw", app_memory=0x20000)
        self.assertEqual(description["limits"], {"app_memory": 0x20000})

    def test_resources_by_tag(self):
        self.assertTrue(sdk_platform.platform_matches("rect-color+mic", ["mic"]))
        self.assertTrue(sdk_platform.platform_matches("rect-color", ["color~rect"]))
        self.assertFalse(sdk_platform.platform_matches("rect-color", ["mic"]))
        self.assertFalse(sdk_platform.platform_matches("rect-color", ["emery"]))
        # What takes a build's own name for "every build" still gets it.
        self.assertTrue(
            sdk_platform.platform_matches("rect-color+mic", ["rect-color+mic"])
        )


class TestFlexibleTargets(unittest.TestCase):
    ALL = ("aplite", "basalt", "chalk", "diorite", "emery", "flint", "gabbro")

    def targets(self, selectors, installed=None):
        return sdk_platform.flexible_targets(selectors, installed or self.ALL)

    def test_every_class_by_default(self):
        # With the platforms of each class, for what does not know classes.
        self.assertEqual(
            self.targets(None),
            ["rect-color", "emery", "rect-bw", "flint", "round-color", "gabbro"],
        )
        self.assertEqual(self.targets([]), self.targets(None))
        self.assertEqual(self.targets(None, ["flint"]), ["rect-bw", "flint"])

    def test_classes(self):
        self.assertEqual(self.targets(["round"]), ["round-color", "gabbro"])
        self.assertEqual(
            self.targets(["rect"]), ["rect-color", "emery", "rect-bw", "flint"]
        )
        self.assertEqual(self.targets(["bw~rect"]), ["rect-bw", "flint"])
        # No platform of the SDK is of that class.
        self.assertEqual(self.targets(["bw~round"]), [])

    def test_required_features(self):
        self.assertEqual(self.targets(["color~rect~mic"]), ["rect-color+mic", "emery"])
        # Flint has no touch: nothing but the flexible binary.
        self.assertEqual(self.targets(["bw~touch"]), ["rect-bw+touch"])
        self.assertEqual(
            self.targets(["touch~mic"]),
            [
                "rect-color+mic+touch",
                "emery",
                "rect-bw+mic+touch",
                "round-color+mic+touch",
                "gabbro",
            ],
        )

    def test_platforms_by_name(self):
        self.assertEqual(
            self.targets(["round", "basalt", "gabbro"]),
            ["round-color", "gabbro", "basalt"],
        )
        # One the SDK does not have.
        self.assertEqual(self.targets(["emery"], ["flint"]), [])

    def test_what_is_neither(self):
        for selector in ("200w", "laser", "color~slate"):
            with self.assertRaises(PlatformError):
                self.targets([selector])
        with self.assertRaises(PlatformError):
            self.targets(None, ["basalt"])


class TestBezelPlatforms(unittest.TestCase):
    ALL = ("aplite", "basalt", "chalk", "diorite", "emery", "flint", "gabbro")

    def test_a_platform_shows_the_largest_binary_that_fits(self):
        report = sdk_platform.bezel_platforms(["aplite", "basalt", "chalk"], self.ALL)
        self.assertEqual(report, {"emery": "basalt", "gabbro": "chalk"})
        # Of two as large, the one of the watch's colour depth.
        report = sdk_platform.bezel_platforms(["basalt", "diorite"], ["emery"])
        self.assertEqual(report, {"emery": "basalt"})

    def test_a_binary_for_the_same_display_fills_it(self):
        # Flint has Diorite's display, and Aplite's.
        self.assertEqual(sdk_platform.bezel_platforms(["diorite"], ["flint"]), {})
        self.assertEqual(sdk_platform.bezel_platforms(["aplite"], ["flint"]), {})

    def test_a_platform_that_runs_none(self):
        report = sdk_platform.bezel_platforms(["emery"], self.ALL)
        self.assertEqual(report, {"flint": None, "gabbro": None})

    def test_nothing_to_say(self):
        self.assertEqual(sdk_platform.bezel_platforms(self.ALL, self.ALL), {})
        # Frozen platforms are not held against a project.
        self.assertEqual(
            sdk_platform.bezel_platforms(["emery"], ["basalt", "emery"]), {}
        )


class TestTargetPlatforms(unittest.TestCase):
    def test_by_name(self):
        self.assertTrue(sdk_platform.platform_matches("emery", ["flint", "emery"]))
        self.assertFalse(sdk_platform.platform_matches("emery", ["flint"]))
        self.assertFalse(sdk_platform.platform_matches("emery", []))

    def test_by_capability(self):
        self.assertTrue(sdk_platform.platform_matches("gabbro", ["round"]))
        self.assertFalse(sdk_platform.platform_matches("emery", ["round"]))
        self.assertTrue(sdk_platform.platform_matches("flint", ["bw", "round"]))
        self.assertTrue(sdk_platform.platform_matches("emery", ["200w"]))
        self.assertTrue(sdk_platform.platform_matches("chalk", ["round"]))

    def test_all_of_several(self):
        self.assertTrue(sdk_platform.platform_matches("emery", ["color~rect"]))
        self.assertFalse(sdk_platform.platform_matches("gabbro", ["color~rect"]))
        self.assertFalse(sdk_platform.platform_matches("flint", ["color~rect"]))
        self.assertTrue(sdk_platform.platform_matches("flint", ["mic~speaker"]))

    def test_unknown_platform(self):
        self.assertTrue(sdk_platform.platform_matches("slate", ["slate"]))
        self.assertFalse(sdk_platform.platform_matches("slate", ["color"]))


if __name__ == "__main__":
    unittest.main()
