# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Sphinx glue between the hand-written pages and the Doxygen reference.

- ``{apidoc}`group``` links to the Doxygen page of a group, read from the
  ``@defgroup`` comments in ``include/``.
- ``pbl-command`` renders the usage and options of a ``pbl`` command
  from its own argument parser.
- ``pbl_redirects`` keeps moved pages reachable at their old URL.
"""

import argparse
import hashlib
import os
import posixpath
import re
import sys
from pathlib import Path

from docutils import nodes
from sphinx.util.docutils import SphinxDirective, SphinxRole

ROOT = Path(__file__).resolve().parents[2]
SOURCES = [ROOT / "include", ROOT / "docs" / "_doxygen"]
PBL_CLI = ROOT / "tools" / "libs" / "pbl-cli"

COMMENT_RE = re.compile(r"/\*\*(.*?)\*/", re.DOTALL)
DEFGROUP_RE = re.compile(r"[@\\]defgroup\s+(\w+)\s+(.+)")


def scan_groups():
    """Title of every Doxygen group, by name."""
    groups = {}
    for base in SOURCES:
        for path in sorted(base.rglob("*")):
            if path.suffix not in (".h", ".dox"):
                continue
            text = path.read_text(errors="replace")
            for comment in COMMENT_RE.findall(text):
                for m in DEFGROUP_RE.finditer(comment):
                    groups[m.group(1)] = m.group(2).strip()
    return groups


def apidoc_uri(docname, group):
    # Doxyfile sets CREATE_SUBDIRS with level 8: pages are spread over
    # d<x>/d<xx>/ directories named after the MD5 of the page name
    page = "group__" + group.replace("_", "__")
    md5 = hashlib.md5(page.encode()).digest()
    path = f"d{md5[14] & 0xF:x}/d{md5[15]:02x}/{page}.html"
    return "../" * docname.count("/") + "apidoc/" + path


class ApidocRole(SphinxRole):
    def run(self):
        m = re.match(r"^(.+?)\s*<(\w+)>$", self.text)
        title, group = (m.group(1), m.group(2)) if m else (None, self.text)
        groups = self.env.app.pbl_groups
        if group not in groups:
            msg = self.inliner.reporter.error(
                f"unknown Doxygen group {group!r}", line=self.lineno
            )
            return [self.inliner.problematic(self.rawtext, self.rawtext, msg)], [msg]
        title = title or groups[group]
        uri = apidoc_uri(self.env.docname, group)
        return [
            nodes.reference(self.rawtext, title, refuri=uri, classes=["apidoc"])
        ], []


def pbl_subparsers():
    sys.path.insert(0, str(PBL_CLI))
    from pbl.app.main import _build_parser
    from pbl.commands import builtin_commands

    parser = _build_parser(builtin_commands(), [], None)
    for action in parser._actions:
        if isinstance(action, argparse._SubParsersAction):
            return action.choices
    raise RuntimeError("pbl has no subcommands")


class PblCommandDirective(SphinxDirective):
    """Usage line and options of a ``pbl`` command, without the shared
    options every command takes."""

    required_arguments = 1

    def run(self):
        name = self.arguments[0]
        commands = self.env.app.pbl_commands
        if name not in commands:
            raise self.error(f"unknown pbl command {name!r}")
        parser = commands[name]
        formatter = parser._get_formatter()

        usage = formatter._format_usage(
            parser.usage, parser._actions, parser._mutually_exclusive_groups, ""
        ).strip()
        result = [nodes.literal_block(usage, usage, language="none")]

        items = nodes.definition_list(classes=["pbl-options"])
        for group in parser._action_groups:
            if group.title == "shared options":
                continue
            for action in group._group_actions:
                if isinstance(action, argparse._HelpAction):
                    continue
                if isinstance(action, argparse._SubParsersAction):
                    for choice in action._choices_actions:
                        items += self._item(choice.dest, choice.help)
                    continue
                items += self._item(
                    self._invocation(formatter, action), formatter._expand_help(action)
                )
        if len(items):
            result.append(items)
        for path in (PBL_CLI / "pbl").rglob("*.py"):
            self.env.note_dependency(str(path))
        return result

    @staticmethod
    def _invocation(formatter, action):
        if not action.option_strings:
            return formatter._metavar_formatter(action, action.dest)(1)[0]
        if action.nargs == 0:
            return ", ".join(action.option_strings)
        metavar = formatter._format_args(action, action.dest.upper())
        return ", ".join(action.option_strings) + " " + metavar

    @staticmethod
    def _item(term, text):
        item = nodes.definition_list_item()
        item += nodes.term("", "", nodes.literal(text=term))
        item += nodes.definition("", nodes.paragraph(text=text or ""))
        return item


REDIRECT_TEMPLATE = """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Redirecting</title>
<link rel="canonical" href="{uri}">
<meta http-equiv="refresh" content="0; url={uri}">
</head>
<body><a href="{uri}">This page has moved.</a></body>
</html>
"""


def write_redirects(app, exception):
    if exception or app.builder.format != "html":
        return
    for old, new in app.config.pbl_redirects.items():
        if old in app.env.found_docs:
            raise ValueError(f"redirect source {old!r} is still a page")
        if new not in app.env.found_docs:
            raise ValueError(f"redirect target {new!r} is not a page")
        uri = (
            posixpath.relpath(new, posixpath.dirname(old) or ".")
            + app.builder.out_suffix
        )
        path = os.path.join(app.outdir, old + app.builder.out_suffix)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(REDIRECT_TEMPLATE.format(uri=uri))


def builder_inited(app):
    app.pbl_groups = scan_groups()
    app.pbl_commands = pbl_subparsers()


def setup(app):
    app.add_config_value("pbl_redirects", {}, "html")
    app.add_role("apidoc", ApidocRole())
    app.add_directive("pbl-command", PblCommandDirective)
    app.connect("builder-inited", builder_inited)
    app.connect("build-finished", write_redirects)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
