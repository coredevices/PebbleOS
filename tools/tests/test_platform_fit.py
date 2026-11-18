# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import json
import os
import sys
import unittest

# Allow us to run even if not at the `tools` directory.
root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(0, root_dir)

import platform_fit
from pebble_sdk_platform import bundle_description

CASES = os.path.join(os.path.dirname(__file__), "platform_fit_cases.json")


class TestPlatformFit(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(CASES) as f:
            cls.table = json.load(f)

    def test_cases(self):
        platforms = self.table["platforms"]
        for case in self.table["cases"]:
            with self.subTest(case["name"]):
                best = platform_fit.best_first(
                    platforms[case["watch"]], [platforms[b] for b in case["binaries"]]
                )
                self.assertEqual([b["name"] for b in best], case["expected"])

    def test_age(self):
        self.assertEqual(tuple(self.table["age"]), platform_fit.AGE)

    def test_known_platforms_are_as_the_sdk_has_them(self):
        for name in platform_fit.AGE:
            description = bundle_description(name)
            del description["version"]
            self.assertEqual(self.table["platforms"][name], description)

    def test_every_platform_is_used(self):
        used = set()
        for case in self.table["cases"]:
            used.add(case["watch"])
            used.update(case["binaries"])
        self.assertEqual(used, set(self.table["platforms"]))


if __name__ == "__main__":
    unittest.main()
