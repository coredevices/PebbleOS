# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Snippets: presets a build adds to the board and the application.

A snippet is a directory under ``snippets/``, in the application or in
PebbleOS, the application's taking precedence:

    snippets/<name>/snippet.conf             every board
    snippets/<name>/snippet.overlay          every board
    snippets/<name>/boards/<board>.conf      one board
    snippets/<name>/boards/<board>.overlay   one board

Its files apply after the application's, in that order. Boards sharing a
preset include the same file from their overlays.
"""

from pathlib import Path

SNIPPETS_DIR = "snippets"


class SnippetError(Exception):
    pass


def find(srcdir, name, appdir=None):
    """The directory of snippet ``name``."""
    roots = ([Path(appdir)] if appdir else []) + [Path(srcdir)]
    for root in roots:
        path = root / SNIPPETS_DIR / name
        if path.is_dir():
            return path
    raise SnippetError(f"no snippet {name} in {', '.join(str(r / SNIPPETS_DIR) for r in roots)}")


def files(srcdir, names, board, appdir=None):
    """``(confs, overlays)`` the snippets ``names`` apply to ``board``."""
    confs, overlays = [], []
    for name in names:
        path = find(srcdir, name, appdir)
        confs += [p for p in (path / "snippet.conf", path / "boards" / f"{board}.conf") if p.exists()]
        overlays += [
            p for p in (path / "snippet.overlay", path / "boards" / f"{board}.overlay") if p.exists()
        ]
    return confs, overlays


def supports(srcdir, name, board, appdir=None):
    """Whether snippet ``name`` has anything for ``board``."""
    confs, overlays = files(srcdir, [name], board, appdir)
    return bool(confs or overlays)
