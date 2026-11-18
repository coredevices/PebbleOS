# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import json
import os
import sys
import tempfile
import unittest
import zipfile

# Allow us to run even if not at the `tools` directory.
root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(0, root_dir)

import mkbundle


class TestMakeWatchappBundle(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.appinfo = self._write("appinfo.json", json.dumps({"uuid": "x"}))
        self.outfile = os.path.join(self.dir.name, "out.pbw")

    def tearDown(self):
        self.dir.cleanup()

    def _write(self, name, content):
        path = os.path.join(self.dir.name, name.replace("/", "_"))
        with open(path, "w") as f:
            f.write(content)
        return path

    def test_plugin_only_bundle_has_no_manifest(self):
        script = self._write("plugin.js", "// plugin")
        mkbundle.make_watchapp_bundle(
            timestamp=0,
            appinfo=self.appinfo,
            binaries=[],
            js=[],
            outfile=self.outfile,
            named_files={"src/plugin/index.js": script},
        )
        with zipfile.ZipFile(self.outfile) as z:
            # A manifest.json at the root would read as an aplite build.
            self.assertEqual(
                sorted(z.namelist()), ["appinfo.json", "src/plugin/index.js"]
            )
            self.assertEqual(z.read("src/plugin/index.js"), b"// plugin")

    def test_named_files_keep_their_paths(self):
        page = self._write("config.html", "<html></html>")
        script = self._write("plugin.js", "// plugin")
        mkbundle.make_watchapp_bundle(
            timestamp=0,
            appinfo=self.appinfo,
            binaries=[],
            js=[],
            outfile=self.outfile,
            named_files={"config.html": page, "src/plugin/index.js": script},
        )
        with zipfile.ZipFile(self.outfile) as z:
            self.assertIn("config.html", z.namelist())
            self.assertIn("src/plugin/index.js", z.namelist())

    def test_binary_manifest_describes_its_platform(self):
        app = self._write("pebble-app.bin", "app")
        resources = self._write("app_resources.pbpack", "resources")
        platform = {"name": "emery", "display": {"width": 200, "height": 228}}
        binary = {
            "watchapp": app,
            "resources": resources,
            "worker_bin": None,
            "layouts": None,
            "sdk_version": {"major": 5, "minor": 86},
        }
        mkbundle.make_watchapp_bundle(
            timestamp=0,
            appinfo=self.appinfo,
            binaries=[
                dict(binary, subfolder="emery", platform=platform),
                dict(binary, subfolder="basalt"),
            ],
            js=[],
            outfile=self.outfile,
        )
        with zipfile.ZipFile(self.outfile) as z:
            self.assertEqual(
                json.loads(z.read("emery/manifest.json"))["platform"], platform
            )
            self.assertNotIn("platform", json.loads(z.read("basalt/manifest.json")))

    def test_nothing_to_bundle(self):
        with self.assertRaises(RuntimeError):
            mkbundle.make_watchapp_bundle(
                timestamp=0,
                appinfo=self.appinfo,
                binaries=[],
                js=[],
                outfile=self.outfile,
            )

    def test_missing_named_file(self):
        with self.assertRaises(mkbundle.MissingFileException):
            mkbundle.make_watchapp_bundle(
                timestamp=0,
                appinfo=self.appinfo,
                binaries=[],
                js=[],
                outfile=self.outfile,
                named_files={"plugin.js": os.path.join(self.dir.name, "nope.js")},
            )


if __name__ == "__main__":
    unittest.main()
