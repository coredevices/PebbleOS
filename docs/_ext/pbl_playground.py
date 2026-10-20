# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The firmware, running in the page.

``pbl-playground`` embeds wasm builds of the firmware, one per board in
``pbl_playground_boards`` (board -> label, the first one shown by default),
with a list to pick one when there are several. The extension builds them
when the HTML builder starts, in build directories that are kept between
runs so only what changed is rebuilt, and publishes them under
``_playground/<board>/``. The page asks for each build's files with
``?v=<hash of the build>``, so no cache ever mixes files from two builds.

``pbl_playground = False`` (or ``-D pbl_playground=0``) leaves the build out,
for faster builds; the page then says the playground is not part of this
build.
"""

import hashlib
import html
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

from docutils import nodes
from sphinx.errors import ExtensionError
from sphinx.util import logging
from sphinx.util.docutils import SphinxDirective

ROOT = Path(__file__).resolve().parents[2]
OUTDIR = "_playground"
FILES = ["pebbleos.html", "pebbleos.js", "pebbleos.wasm", "pebbleos.data", "*.png"]
VERSIONED = ["pebbleos.js", "pebbleos.wasm", "pebbleos.data"]

logger = logging.getLogger(__name__)

IFRAME = """\
<div class="pbl-playground">
  <select aria-label="Board"{hidden}>{options}</select>
  <iframe title="PebbleOS playground" loading="lazy" allow="fullscreen"></iframe>
</div>
<script>
(() => {{
  const root = document.currentScript.previousElementSibling;
  const frame = root.querySelector("iframe");
  const select = root.querySelector("select");
  const base = {base};
  const versions = {versions};
  const theme = () =>
    document.documentElement.classList.contains("dark") ? "dark" : "light";
  const load = () => {{
    frame.src = base + select.value + "/pebbleos.html?v=" + versions[select.value] +
      "&embed&theme=" + theme();
  }};
  try {{
    const saved = localStorage.getItem("pbl-playground-board");
    if ([...select.options].some((o) => o.value === saved)) select.value = saved;
  }} catch (e) {{}}
  select.addEventListener("change", () => {{
    try {{ localStorage.setItem("pbl-playground-board", select.value); }} catch (e) {{}}
    // The keys go to the new watch, not the list.
    frame.addEventListener("load", () => frame.focus({{ preventScroll: true }}), {{ once: true }});
    load();
  }});
  load();
  new MutationObserver(() => frame.contentWindow?.postMessage({{ theme: theme() }}, "*"))
    .observe(document.documentElement, {{ attributes: true, attributeFilter: ["class"] }});
  // The page says how tall it is, so the frame never scrolls.
  window.addEventListener("message", (e) => {{
    if (e.source === frame.contentWindow && e.data?.pblPlaygroundHeight) {{
      frame.style.height = e.data.pblPlaygroundHeight + "px";
    }}
  }});
}})();
</script>
"""


def _enabled(app):
    return app.config.pbl_playground and app.builder.format == "html"


def _dirname(board):
    return board.replace("/", "_").replace("@", "_")


class PlaygroundDirective(SphinxDirective):
    def run(self):
        if not self.config.pbl_playground:
            note = nodes.note()
            note += nodes.paragraph(
                text="The playground is not part of this build of the documentation."
            )
            return [note]
        depth = self.env.docname.count("/")
        boards = self.config.pbl_playground_boards
        for path in self.env.pbl_playground_deps:
            self.env.note_dependency(path)
        options = "".join(
            f'<option value="{_dirname(b)}">{html.escape(label)}</option>'
            for b, label in boards.items()
        )
        content = IFRAME.format(
            base=json.dumps("../" * depth + f"{OUTDIR}/"),
            versions=json.dumps(self.env.pbl_playground_versions),
            options=options,
            hidden=" hidden" if len(boards) < 2 else "",
        )
        return [nodes.raw("", content, format="html")]


def build_firmware(app):
    if not _enabled(app):
        return
    if not app.config.pbl_playground_boards:
        raise ExtensionError("pbl_playground_boards is empty")
    topdir = Path(app.config.pbl_playground_build_dir)
    if not topdir.is_absolute():
        topdir = Path(app.confdir) / topdir

    def run(*cmd):
        result = subprocess.run(
            cmd, cwd=ROOT, capture_output=True, text=True, check=False
        )
        if result.returncode != 0:
            raise ExtensionError(
                f"playground build failed: {' '.join(cmd)}\n"
                + result.stdout[-4000:]
                + result.stderr[-4000:]
            )

    app.pbl_playground_dirs = {}
    for board in app.config.pbl_playground_boards:
        builddir = topdir / _dirname(board)
        if not (builddir / "CMakeCache.txt").exists():
            logger.info("configuring the playground (%s) in %s", board, builddir)
            run(
                "cmake",
                "-S",
                str(ROOT),
                "-B",
                str(builddir),
                "-G",
                "Ninja",
                f"-DBOARD={board}",
                f"-DPython3_EXECUTABLE={sys.executable}",
                # The page downloads the module: no DWARF, a fraction of the size.
                "-DCONFIG_DEBUG_INFO=n",
            )
        logger.info("building the playground (%s)", board)
        run("cmake", "--build", str(builddir))
        app.pbl_playground_dirs[_dirname(board)] = builddir

    app.env.pbl_playground_versions = {}
    app.env.pbl_playground_deps = []
    for name, builddir in app.pbl_playground_dirs.items():
        digest = hashlib.sha256()
        for f in VERSIONED:
            digest.update((builddir / f).read_bytes())
            app.env.pbl_playground_deps.append(str(builddir / f))
        app.env.pbl_playground_versions[name] = digest.hexdigest()[:16]


def publish_firmware(app, exception):
    if exception is not None or not _enabled(app):
        return
    for name, builddir in app.pbl_playground_dirs.items():
        outdir = Path(app.outdir) / OUTDIR / name
        os.makedirs(outdir, exist_ok=True)
        for pattern in FILES:
            for path in builddir.glob(pattern):
                shutil.copy2(path, outdir / path.name)
        page = outdir / "pebbleos.html"
        version = app.env.pbl_playground_versions[name]
        text, n = re.subn(
            r"""src=(["']?)pebbleos\.js\1""",
            f'src="pebbleos.js?v={version}"',
            page.read_text(),
        )
        if n != 1:
            raise ExtensionError(f"playground: no pebbleos.js script in {page}")
        page.write_text(text)


def setup(app):
    app.add_config_value(
        "pbl_playground",
        True,
        "env",
        bool,
    )
    app.add_config_value(
        "pbl_playground_boards",
        {"native_emery/wasm": "Pebble Time 2"},
        "env",
        dict,
    )
    app.add_config_value("pbl_playground_build_dir", "_build/playground", "env", str)
    app.add_directive("pbl-playground", PlaygroundDirective)
    app.connect("builder-inited", build_firmware)
    app.connect("build-finished", publish_firmware)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
