# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "sdk" / "tools"))

from pebble_project import undeclared_health_error, undeclared_moddable_health_error


class TestUndeclaredHealthError(unittest.TestCase):
    SYMBOLS = ("main", "health_service_sum_today", "app_event_loop")

    def test_only_apps_using_health_need_it(self):
        self.assertIsNone(undeclared_health_error({}, ["main", "app_event_loop"]))
        units = ["main", "health_service_get_measurement_system_for_display"]
        self.assertIsNone(undeclared_health_error({}, units))

    def test_declaring_health_allows_it(self):
        self.assertIsNone(undeclared_health_error({"usesPermissions": ["Health"]}, self.SYMBOLS))
        info = {"usesPermissions": [{"name": "Health", "reason": "Shows your steps"}]}
        self.assertIsNone(undeclared_health_error(info, self.SYMBOLS))

    def test_using_health_without_declaring_it_is_an_error(self):
        error = undeclared_health_error({"usesPermissions": ["Location"]}, self.SYMBOLS)
        self.assertIn("health_service_sum_today", error)
        self.assertIn("'Health'", error)

    def test_a_legacy_manifest_cannot_use_health(self):
        info = {"manifestVersion": 1, "usesPermissions": ["Health"]}
        self.assertIn("manifestVersion 1", undeclared_health_error(info, self.SYMBOLS))


class TestUndeclaredModdableHealthError(unittest.TestCase):
    # Compiled with xsc and xsl as mcrun does: one imports pebble/health and listens for "health";
    # the other only has a "health" property and a "pebble/healthy" string.
    USES = (Path(__file__).parent / "moddable_uses_health.xsa").read_bytes()
    NO_HEALTH = (Path(__file__).parent / "moddable_no_health.xsa").read_bytes()

    def test_javascript_using_health_needs_it(self):
        self.assertIn('"pebble/health"', undeclared_moddable_health_error({}, self.USES))
        self.assertIsNone(undeclared_moddable_health_error({"usesPermissions": ["Health"]}, self.USES))

    def test_the_health_event_needs_it(self):
        mod = self.USES.replace(b"\x0epebble/health\x00", b"\x0epebble/other\x00\x00")
        self.assertIn('"health" event', undeclared_moddable_health_error({}, mod))

    def test_lookalikes_are_not_health(self):
        self.assertIsNone(undeclared_moddable_health_error({}, self.NO_HEALTH))


if __name__ == "__main__":
    unittest.main()
