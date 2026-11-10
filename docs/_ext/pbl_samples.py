# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The ``pebbleos-sample`` directive, the body of a sample's page.

.. code-block:: markdown

    ```{pebbleos-sample}
    :path: samples/kernel/mutex
    :sources: src/main.c
    :output:
    ```

It shows how to run the sample and the boards it is for, read from its
``sample.yml``, then a *Sources* section with the files named in
``:sources:`` (none, one or several, relative to the sample) and, with
``:output:``, an *Output* section with the lines its ``sample.yml``
expects it to print, as the regular expressions they are.
``:path:`` is relative to ``pebbleos_samples_root``, the root of the
checkout unless ``conf.py`` sets it.
"""

import sys
from pathlib import Path
from typing import ClassVar

from docutils import nodes
from docutils.parsers.rst import directives
from sphinx.directives.code import container_wrapper
from sphinx.util.docutils import SphinxDirective

ROOT = Path(__file__).resolve().parents[2]
PBL_CLI = ROOT / "tools" / "libs" / "pbl-cli"

LANGUAGES = {
    ".c": "c",
    ".h": "c",
    ".cmake": "cmake",
    ".py": "python",
    ".yml": "yaml",
    ".yaml": "yaml",
}


def _samples():
    if str(PBL_CLI) not in sys.path:
        sys.path.insert(0, str(PBL_CLI))
    from pbl import samples

    return samples


def _language(path):
    if path.name == "CMakeLists.txt":
        return "cmake"
    return LANGUAGES.get(path.suffix, "none")


def _without_license(text):
    """The file without the SPDX lines it starts with, nor the blank line
    after them."""
    lines = text.splitlines()
    while lines and "SPDX-" in lines[0]:
        lines.pop(0)
    while lines and not lines[0].strip():
        lines.pop(0)
    return "\n".join(lines) + "\n"


def _boards(sample):
    if sample.boards:
        text = "only " + ", ".join(sample.boards)
    else:
        text = "every board"
    if sample.skip:
        text += " but " + ", ".join(sample.skip)
    return text


class SampleDirective(SphinxDirective):
    option_spec: ClassVar = {
        "path": directives.unchanged_required,
        "sources": directives.unchanged,
        "output": directives.flag,
    }

    def run(self):
        if "path" not in self.options:
            raise self.error("pebbleos-sample needs a :path:")

        root = Path(self.config.pebbleos_samples_root or ROOT)
        if not root.is_absolute():
            root = Path(self.env.app.confdir) / root
        path = (root / self.options["path"]).resolve()

        samples = _samples()
        manifest = path / samples.SAMPLE_FILE
        try:
            sample = samples.load(path)
        except samples.SampleError as e:
            raise self.error(str(e)) from e
        self.env.note_dependency(str(manifest))

        try:
            run = path.relative_to(ROOT)
        except ValueError:
            run = path

        result = [self._fields(f"pbl samples {run}", _boards(sample))]

        sources = (self.options.get("sources") or "").split()
        if sources:
            section = self._section("Sources")
            for name in sources:
                section += self._source(path, name)
            result.append(section)

        if "output" in self.options:
            section = self._section("Output")
            text = "\n".join(sample.expect)
            section += nodes.literal_block(text, text, language="none")
            result.append(section)

        return result

    def _fields(self, command, boards):
        fields = nodes.field_list(classes=["pebbleos-sample"])
        for name, body in (
            ("Run", nodes.literal(text=command)),
            ("Boards", nodes.Text(boards)),
        ):
            field = nodes.field()
            field += nodes.field_name(text=name)
            field += nodes.field_body("", nodes.paragraph("", "", body))
            fields += field
        return fields

    def _section(self, title):
        section = nodes.section()
        section += nodes.title(text=title)
        section["names"].append(nodes.fully_normalize_name(title))
        self.state.document.note_implicit_target(section, section)
        return section

    def _source(self, sample_path, name):
        path = (sample_path / name).resolve()
        if not path.is_file():
            raise self.error(f"no source {name} in {sample_path}")
        self.env.note_dependency(str(path))

        text = _without_license(path.read_text())
        block = nodes.literal_block(text, text, language=_language(path))
        block["source"] = str(path)
        self.set_source_info(block)
        return container_wrapper(self, block, name)


def setup(app):
    app.add_config_value("pebbleos_samples_root", None, "env", types=[str, type(None)])
    app.add_directive("pebbleos-sample", SampleDirective)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
