# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools import snippets


def make(root, name, *files):
    for f in files:
        path = root / "snippets" / name / f
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("")


def test_files_for_every_board_then_the_board(tmp_path):
    make(tmp_path, "fs", "snippet.conf", "snippet.overlay", "boards/a.conf", "boards/a.overlay")
    confs, overlays = snippets.files(tmp_path, ["fs"], "a")
    assert [p.name for p in confs] == ["snippet.conf", "a.conf"]
    assert [p.name for p in overlays] == ["snippet.overlay", "a.overlay"]


def test_board_without_files_gets_nothing(tmp_path):
    make(tmp_path, "fs", "boards/a.overlay")
    assert snippets.files(tmp_path, ["fs"], "b") == ([], [])
    assert snippets.supports(tmp_path, "fs", "a")
    assert not snippets.supports(tmp_path, "fs", "b")


def test_application_snippet_wins(tmp_path):
    src, app = tmp_path / "src", tmp_path / "app"
    make(src, "fs", "snippet.conf")
    make(app, "fs", "boards/a.conf")
    confs, _ = snippets.files(src, ["fs"], "a", app)
    assert confs == [app / "snippets" / "fs" / "boards" / "a.conf"]


def test_unknown_snippet_fails(tmp_path):
    with pytest.raises(snippets.SnippetError, match="no snippet nope"):
        snippets.files(tmp_path, ["nope"], "a")
