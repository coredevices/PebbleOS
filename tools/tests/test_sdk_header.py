# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""What the SDK header leaves to the defines of the app build."""

import json
import os
import sys
import unittest

root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(0, root_dir)
sys.path.insert(0, os.path.join(root_dir, "generate_native_sdk"))

import pebble_sdk_platform as sdk_platform
from generate_app_header import active_lines, define_text
from pebble_sdk_platform import pebble_platforms

SYMBOLS = os.path.join(root_dir, "generate_native_sdk", "exported_symbols.json")

SLATE = {
    "version": 1,
    "name": "slate",
    "abi": "armv7m",
    "display": {"width": 240, "height": 240, "shape": "rect", "color": True},
    "limits": dict(pebble_platforms["emery"]["DESCRIPTOR"]["limits"]),
}


class Define:
    def __init__(
        self,
        name,
        full_definition,
        selected_by=None,
        sdk_definition=None,
        runtime_feature=None,
    ):
        self.name = name
        self.full_definition = full_definition
        self.selected_by = selected_by
        self.sdk_definition = sdk_definition
        self.runtime_feature = runtime_feature


def _defines(exports):
    for e in exports:
        if e["type"] == "group":
            yield from _defines(e["exports"])
        elif e["type"] == "define":
            yield e


def _defined(platform):
    return {d.split("=")[0] for d in platform["DEFINES"]}


class TestDefineText(unittest.TestCase):
    def test_plain(self):
        e = Define("FOO", "#define FOO 1")
        self.assertEqual(define_text(e, shared=True), "#define FOO 1")

    def test_selected(self):
        e = Define(
            "COLOR_FALLBACK", "#define COLOR_FALLBACK(color, bw)   (bw)", "PBL_COLOR"
        )
        self.assertEqual(
            define_text(e, shared=True).splitlines(),
            [
                "#if defined(PBL_COLOR)",
                "#define COLOR_FALLBACK(color, bw) (color)",
                "#else",
                "#define COLOR_FALLBACK(color, bw) (bw)",
                "#endif",
            ],
        )
        # A header of one platform keeps the definition of that platform.
        self.assertEqual(define_text(e, shared=False), e.full_definition)

    def test_feature_asked_at_run_time(self):
        e = Define(
            "PBL_IF_MICROPHONE_ELSE",
            "#define PBL_IF_MICROPHONE_ELSE(if_true, if_false) (if_true)",
            "PBL_MICROPHONE",
            runtime_feature="PlatformFeatureMicrophone",
        )
        self.assertEqual(
            define_text(e, shared=True).splitlines(),
            [
                "#if defined(PBL_MICROPHONE)",
                "#define PBL_IF_MICROPHONE_ELSE(if_true, if_false) (if_true)",
                "#elif defined(PBL_FLEXIBLE)",
                "#define PBL_IF_MICROPHONE_ELSE(if_true, if_false) \\",
                "  (platform_has_feature(PlatformFeatureMicrophone) ? (if_true) : (if_false))",
                "#else",
                "#define PBL_IF_MICROPHONE_ELSE(if_true, if_false) (if_false)",
                "#endif",
            ],
        )

    def test_selected_has_to_pick_an_argument(self):
        e = Define("FOO", "#define FOO(a, b) (a + b)", "PBL_COLOR")
        with self.assertRaises(RuntimeError):
            define_text(e, shared=True)

    def test_active_lines(self):
        lines = ["#if defined(A)", "a", "#elif defined(B)", "b", "#else", "c", "#endif"]
        self.assertEqual(active_lines(lines, {"A", "B"}), ["a"])
        self.assertEqual(active_lines(lines, {"B"}), ["b"])
        self.assertEqual(active_lines(lines, set()), ["c"])
        with self.assertRaises(RuntimeError):
            active_lines(["#if A > 1", "a", "#endif"], set())


class TestExportedDefines(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(SYMBOLS) as f:
            cls.defines = {e["name"]: e for e in _defines(json.load(f)["exports"])}

    def test_selectors_are_defines_of_a_platform(self):
        known = {"PBL_COLOR", "PBL_BW", "PBL_RECT", "PBL_ROUND"}
        known |= {define for define, _ in sdk_platform.FEATURES.values()}
        for e in self.defines.values():
            if "selectedBy" in e:
                self.assertIn(e["selectedBy"], known, e["name"])

    def _platform_type(self, platform):
        lines = self.defines["PBL_PLATFORM_TYPE_CURRENT"]["sdkDefinition"]
        (line,) = active_lines(lines, _defined(platform))
        return line.split()[-1]

    def test_platform_type(self):
        for name in (
            "aplite",
            "basalt",
            "chalk",
            "diorite",
            "emery",
            "flint",
            "gabbro",
        ):
            self.assertEqual(
                self._platform_type(pebble_platforms[name]),
                "PlatformType" + name.capitalize(),
            )

    def test_platform_type_of_a_new_platform(self):
        # The one it is most like: by shape, then colour depth.
        def of(**display):
            desc = dict(SLATE, display=dict(SLATE["display"], **display))
            return self._platform_type(sdk_platform.platform_from_descriptor(desc))

        self.assertEqual(of(), "PlatformTypeEmery")
        self.assertEqual(of(color=False), "PlatformTypeFlint")
        self.assertEqual(of(shape="round"), "PlatformTypeGabbro")
        self.assertEqual(of(shape="round", color=False), "PlatformTypeGabbro")

    def test_platform_type_of_a_flexible_build(self):
        def of(shape, color):
            return self._platform_type(sdk_platform.flexible_build(shape, color))

        self.assertEqual(of("rect", True), "PlatformTypeEmery")
        self.assertEqual(of("rect", False), "PlatformTypeFlint")
        self.assertEqual(of("round", True), "PlatformTypeGabbro")

    def test_run_time_features_are_features(self):
        # PlatformFeature<Name> counts the features in the table's order.
        names = [n.replace("_", "") for n in sdk_platform.FEATURES]
        for e in self.defines.values():
            if "runtimeFeature" in e:
                feature = e["runtimeFeature"].replace("PlatformFeature", "").lower()
                self.assertIn(feature, names, e["name"])
                self.assertEqual(
                    sdk_platform.FEATURES[
                        list(sdk_platform.FEATURES)[names.index(feature)]
                    ][0],
                    e["selectedBy"],
                )


if __name__ == "__main__":
    unittest.main()
