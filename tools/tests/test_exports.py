# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import json
import os
import sys
import tempfile
import unittest
from functools import cmp_to_key

root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(0, os.path.join(root_dir, "generate_native_sdk"))

import exports

EXPORTS = {
    "revision": "112",
    "files": [],
    "exports": [
        {
            "type": "group",
            "name": "Time",
            "exports": [
                {
                    "type": "function",
                    "name": "time",
                    "implName": "time_time32",
                    "addedRevision": "51",
                    "supersededRevision": "112",
                },
                {
                    "type": "function",
                    "name": "time",
                    "implName": "time_impl",
                    "addedRevision": "112",
                },
                {"type": "function", "name": "other", "addedRevision": "60"},
                {
                    "type": "function",
                    "name": "glance",
                    "implName": "glance_time32",
                    "addedRevision": "83",
                    "supersededRevision": "112",
                    "stubReturn": "1",
                },
                {
                    "type": "function",
                    "name": "glance",
                    "addedRevision": "112",
                    "stubReturn": "1",
                },
            ],
        }
    ],
}


def _functions(frozen_revision=None):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(EXPORTS, f)
    try:
        _, tree = exports.parse_export_file(
            f.name, False, frozen_revision=frozen_revision
        )
    finally:
        os.unlink(f.name)
    functions = []
    exports.walk_tree(
        tree,
        lambda e: (
            functions.append(e)
            if isinstance(e, (exports.FunctionExport, exports.StubbedFunctionExport))
            else None
        ),
    )
    return functions


def _declared(functions):
    """Names that end up declared in pebble.h."""
    return sorted(
        f.name
        for f in functions
        if not f.deprecated
        and not f.skip_definition
        and not getattr(f, "removed", False)
    )


def _table(functions):
    """Jump table order, as the generator lays it out."""
    table = [f for f in functions if isinstance(f, exports.FunctionExport)]

    def cmp(x, y):
        a = (x.added_revision, x.sort_name)
        b = (y.added_revision, y.sort_name)
        return (a > b) - (a < b)

    return [(f.name, f.impl_name) for f in sorted(table, key=cmp_to_key(cmp))]


class TestSupersede(unittest.TestCase):
    def test_current_platform(self):
        functions = _functions()
        self.assertEqual(_declared(functions), ["glance", "other", "time"])
        # The old slots keep their place for existing apps, out of the headers.
        self.assertEqual(
            _table(functions),
            [
                ("time__pre112", "time_time32"),
                ("other", "other"),
                ("glance__pre112", "glance_time32"),
                ("glance", "glance"),
                ("time", "time_impl"),
            ],
        )

    def test_frozen_platform_keeps_old_entry(self):
        functions = _functions(frozen_revision=89)
        self.assertEqual(_declared(functions), ["glance", "other", "time"])
        self.assertEqual(
            _table(functions),
            [("time", "time_time32"), ("other", "other"), ("glance", "glance_time32")],
        )
        old_time = next(
            f for f in functions if f.name == "time" and f.added_revision == 51
        )
        # Declared and documented like the replacement.
        self.assertEqual(old_time.declared_as, "time_impl")

    def test_frozen_before_both_keeps_one_stub(self):
        functions = _functions(frozen_revision=81)
        stubs = [
            f
            for f in functions
            if isinstance(f, exports.StubbedFunctionExport) and not f.skip_definition
        ]
        self.assertEqual([(f.name, f.added_revision) for f in stubs], [("glance", 83)])
        self.assertEqual(_declared(functions), ["glance", "other", "time"])


if __name__ == "__main__":
    unittest.main()
