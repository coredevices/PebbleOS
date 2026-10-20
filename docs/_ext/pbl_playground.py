# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The firmware, running in the page.

``pbl-playground`` embeds the ``native_emery/wasm`` build of the firmware.
The extension builds it when the HTML builder starts, in a build directory
that is kept between runs so only what changed is rebuilt, and publishes it
under ``_playground/``.

``pbl_playground = False`` (or ``-D pbl_playground=0``) leaves the build out,
for faster builds; the page then says the playground is not part of this
build.
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

from docutils import nodes
from sphinx.errors import ExtensionError
from sphinx.util import logging
from sphinx.util.docutils import SphinxDirective

ROOT = Path(__file__).resolve().parents[2]
BOARD = "native_emery/wasm"
OUTDIR = "_playground"
FILES = [
    "pebbleos.html",
    "pebbleos.js",
    "pebbleos.wasm",
    "pebbleos.data",
    "pt2-br.png",
    "pt2-sb.png",
]

logger = logging.getLogger(__name__)

IFRAME = """\
<div class="pbl-playground">
  <iframe title="PebbleOS playground" data-src="{src}" loading="lazy"
          allow="fullscreen"></iframe>
</div>
<script>
(() => {{
  const frame = document.currentScript.previousElementSibling.querySelector("iframe");
  const theme = () =>
    document.documentElement.classList.contains("dark") ? "dark" : "light";
  frame.src = frame.dataset.src + "?embed&theme=" + theme();
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


class PlaygroundDirective(SphinxDirective):
    def run(self):
        if not self.config.pbl_playground:
            note = nodes.note()
            note += nodes.paragraph(
                text="The playground is not part of this build of the documentation."
            )
            return [note]
        depth = self.env.docname.count("/")
        src = "../" * depth + f"{OUTDIR}/pebbleos.html"
        return [nodes.raw("", IFRAME.format(src=src), format="html")]


def build_firmware(app):
    if not _enabled(app):
        return
    builddir = Path(app.config.pbl_playground_build_dir)
    if not builddir.is_absolute():
        builddir = Path(app.confdir) / builddir

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

    if not (builddir / "CMakeCache.txt").exists():
        logger.info("configuring the playground (%s) in %s", BOARD, builddir)
        run(
            "cmake",
            "-S",
            str(ROOT),
            "-B",
            str(builddir),
            "-G",
            "Ninja",
            f"-DBOARD={BOARD}",
            f"-DPython3_EXECUTABLE={sys.executable}",
            # The page downloads the module: no DWARF, a fraction of the size.
            "-DCONFIG_DEBUG_INFO=n",
        )
    logger.info("building the playground")
    run("cmake", "--build", str(builddir))
    app.pbl_playground_dir = builddir


def publish_firmware(app, exception):
    if exception is not None or not _enabled(app):
        return
    outdir = Path(app.outdir) / OUTDIR
    os.makedirs(outdir, exist_ok=True)
    for name in FILES:
        shutil.copy2(app.pbl_playground_dir / name, outdir / name)


def setup(app):
    app.add_config_value(
        "pbl_playground",
        True,
        "env",
        bool,
    )
    app.add_config_value("pbl_playground_build_dir", "_build/playground", "env", str)
    app.add_directive("pbl-playground", PlaygroundDirective)
    app.connect("builder-inited", build_firmware)
    app.connect("build-finished", publish_firmware)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
