# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "sdk" / "tools"))

from pebble_project import (
    undeclared_alarm_error,
    undeclared_health_error,
    undeclared_moddable_health_error,
)


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


class TestUndeclaredAlarmError(unittest.TestCase):
    READ_CALLS = ("alarm_service_get", "alarm_service_get_ids", "alarm_service_peek_next")
    WRITE_CALLS = (
        "alarm_service_create",
        "alarm_service_set_enabled",
        "alarm_service_update",
        "alarm_service_delete",
    )
    ALL_CALLS = READ_CALLS + WRITE_CALLS

    def test_only_apps_using_alarm_service_need_it(self):
        self.assertIsNone(undeclared_alarm_error({}, ["main", "app_event_loop"]))
        self.assertIsNone(undeclared_alarm_error({}, ["alarm_service_can_create"]))
        for symbol in self.ALL_CALLS:
            self.assertIn(symbol, undeclared_alarm_error({}, [symbol]))

    def test_read_permission_allows_read_calls(self):
        for symbol in self.READ_CALLS:
            self.assertIsNone(
                undeclared_alarm_error({"usesPermissions": ["AlarmsRead"]}, [symbol])
            )
            info = {"usesPermissions": [{"name": "AlarmsRead", "reason": "Shows your alarms"}]}
            self.assertIsNone(undeclared_alarm_error(info, [symbol]))

            error = undeclared_alarm_error({"usesPermissions": ["AlarmsWrite"]}, [symbol])
            self.assertIn(symbol, error)
            self.assertIn("'AlarmsRead'", error)

    def test_write_permission_allows_write_calls(self):
        for symbol in self.WRITE_CALLS:
            self.assertIsNone(
                undeclared_alarm_error({"usesPermissions": ["AlarmsWrite"]}, [symbol])
            )
            info = {"usesPermissions": [{"name": "AlarmsWrite", "reason": "Creates an alarm"}]}
            self.assertIsNone(undeclared_alarm_error(info, [symbol]))

            error = undeclared_alarm_error({"usesPermissions": ["AlarmsRead"]}, [symbol])
            self.assertIn(symbol, error)
            self.assertIn("'AlarmsWrite'", error)

    def test_both_permissions_allow_every_call(self):
        info = {"usesPermissions": ["AlarmsRead", "AlarmsWrite"]}
        for symbol in self.ALL_CALLS:
            self.assertIsNone(undeclared_alarm_error(info, [symbol]))

    def test_a_mixed_read_and_write_app_needs_both_permissions(self):
        both = {"usesPermissions": ["AlarmsRead", "AlarmsWrite"]}
        for read_symbol in self.READ_CALLS:
            for write_symbol in self.WRITE_CALLS:
                for symbols in ([read_symbol, write_symbol], [write_symbol, read_symbol]):
                    self.assertIsNone(undeclared_alarm_error(both, symbols))

                    error = undeclared_alarm_error({"usesPermissions": ["AlarmsRead"]}, symbols)
                    self.assertIn(write_symbol, error)
                    self.assertIn("'AlarmsWrite'", error)

                    error = undeclared_alarm_error({"usesPermissions": ["AlarmsWrite"]}, symbols)
                    self.assertIn(read_symbol, error)
                    self.assertIn("'AlarmsRead'", error)

    def test_a_legacy_manifest_cannot_use_the_alarm_service(self):
        for symbol, permission in (
            ("alarm_service_get", "AlarmsRead"),
            ("alarm_service_create", "AlarmsWrite"),
        ):
            info = {"manifestVersion": 1, "usesPermissions": [permission]}
            error = undeclared_alarm_error(info, [symbol])
            self.assertIn("manifestVersion 1", error)
            self.assertIn(f"'{permission}'", error)

    def test_compiler_suffixed_symbols_still_count(self):
        for symbol, permission in (
            ("alarm_service_get", "AlarmsRead"),
            ("alarm_service_create", "AlarmsWrite"),
        ):
            for suffix in (
                ".lto_priv.0",
                ".constprop.0",
                ".isra.0",
                ".part.0",
                ".cold",
                ".lto_priv.0.cold",
            ):
                error = undeclared_alarm_error({}, [f"{symbol}{suffix}"])
                self.assertIn(symbol, error)
                self.assertIn(f"'{permission}'", error)

        for symbol in (
            "my_alarm_service_get",
            "alarm_service_get_backup",
            "alarm_service_gets",
        ):
            self.assertIsNone(undeclared_alarm_error({}, [symbol]))


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
