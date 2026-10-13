# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Sphinx glue between the hand-written pages and the Doxygen reference.

- ``{apidoc}`group``` links to the Doxygen page of a group.
- ``apigroups`` renders the children of a Doxygen group as a table, read
  from the ``@defgroup``/``@ingroup``/``@brief`` comments in ``include/``.
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
from dataclasses import dataclass
from pathlib import Path

from docutils import nodes
from sphinx.addnodes import pending_xref
from sphinx.util.docutils import SphinxDirective, SphinxRole

ROOT = Path(__file__).resolve().parents[2]
SOURCES = [ROOT / "include", ROOT / "docs" / "_doxygen"]
PBL_CLI = ROOT / "tools" / "libs" / "pbl-cli"

COMMENT_RE = re.compile(r"/\*\*(.*?)\*/", re.DOTALL)
DEFGROUP_RE = re.compile(r"[@\\]defgroup\s+(\w+)\s+(.+)")
INGROUP_RE = re.compile(r"[@\\]ingroup\s+(\w+)")
BRIEF_RE = re.compile(
    r"[@\\]brief\s+(.+?)(?:\n\s*\*?\s*\n|\n\s*\*?\s*[@\\]|$)", re.DOTALL
)
COMMAND_RE = re.compile(r"[@\\](?:c|p|ref)\s+(\S+)")


@dataclass
class Group:
    name: str
    title: str
    parent: str | None
    brief: str
    header: str | None


def _clean(text):
    text = re.sub(r"\n\s*\*", " ", text)
    text = COMMAND_RE.sub(r"\1", text)
    return " ".join(text.split())


def scan_groups():
    groups = {}
    for base in SOURCES:
        for path in sorted(base.rglob("*")):
            if path.suffix not in (".h", ".dox"):
                continue
            text = path.read_text(errors="replace")
            for comment in COMMENT_RE.findall(text):
                for block in re.split(r"(?=[@\\]defgroup\s)", comment):
                    m = DEFGROUP_RE.search(block)
                    if not m:
                        continue
                    ingroup = INGROUP_RE.search(block)
                    brief = BRIEF_RE.search(block)
                    header = None
                    if path.suffix == ".h":
                        header = path.relative_to(ROOT / "include").as_posix()
                        # A group spanning a directory is named after it
                        if path.stem not in m.group(1):
                            d = posixpath.dirname(header)
                            while "/" in d and posixpath.basename(d) not in m.group(1):
                                d = posixpath.dirname(d)
                            header = d + "/"
                    groups[m.group(1)] = Group(
                        name=m.group(1),
                        title=m.group(2).strip(),
                        parent=ingroup.group(1) if ingroup else None,
                        brief=_clean(brief.group(1)) if brief else "",
                        header=header,
                    )
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
        title = title or groups[group].title
        uri = apidoc_uri(self.env.docname, group)
        return [
            nodes.reference(self.rawtext, title, refuri=uri, classes=["apidoc"])
        ], []


class ApiGroupsDirective(SphinxDirective):
    """Table of the child groups of a Doxygen group.

    Each content line ``group: docname`` adds a link to the guide page
    that covers that group.
    """

    required_arguments = 1
    has_content = True

    def run(self):
        parent = self.arguments[0]
        groups = self.env.app.pbl_groups
        if parent not in groups:
            raise self.error(f"unknown Doxygen group {parent!r}")
        guides = {}
        for line in self.content:
            if line.strip():
                group, _, doc = line.partition(":")
                guides[group.strip()] = doc.strip()

        children = sorted(
            (g for g in groups.values() if g.parent == parent),
            key=lambda g: g.title.lower(),
        )
        has_guides = bool(guides)
        headers = ["Module", "Header", "Description"] + (
            ["Guide"] if has_guides else []
        )

        table = nodes.table(classes=["apigroups"])
        tgroup = nodes.tgroup(cols=len(headers))
        table += tgroup
        for width in [20, 30, 50] + ([12] if has_guides else []):
            tgroup += nodes.colspec(colwidth=width)
        thead = nodes.thead()
        tgroup += thead
        row = nodes.row()
        for h in headers:
            row += nodes.entry("", nodes.paragraph(text=h))
        thead += row
        tbody = nodes.tbody()
        tgroup += tbody

        for g in children:
            row = nodes.row()
            link = nodes.reference(
                "",
                g.title,
                refuri=apidoc_uri(self.env.docname, g.name),
                classes=["apidoc"],
            )
            row += nodes.entry("", nodes.paragraph("", "", link))
            header = nodes.paragraph()
            if g.header:
                header += nodes.literal(text=g.header)
            row += nodes.entry("", header)
            row += nodes.entry("", nodes.paragraph(text=g.brief))
            if has_guides:
                guide = nodes.paragraph()
                if g.name in guides:
                    xref = self._doc_ref(guides.pop(g.name))
                    guide += xref
                row += nodes.entry("", guide)
            tbody += row

        if guides:
            raise self.error(
                f"guides for groups not under {parent!r}: {', '.join(guides)}"
            )

        for g in children:
            if g.header:
                self.env.note_dependency(str(ROOT / "include" / g.header))
        return [table]

    def _doc_ref(self, target):
        ref = pending_xref(
            "",
            refdomain="std",
            reftype="doc",
            reftarget=target,
            refexplicit=True,
            refdoc=self.env.docname,
            refwarn=True,
        )
        ref += nodes.inline(text="Guide")
        return ref


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
    app.add_directive("apigroups", ApiGroupsDirective)
    app.add_directive("pbl-command", PblCommandDirective)
    app.connect("builder-inited", builder_inited)
    app.connect("build-finished", write_redirects)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
