# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools import boards


def board(srcdir, name, manifest=""):
    directory = srcdir / "boards" / name
    directory.mkdir(parents=True)
    (directory / "defconfig").write_text("")
    (directory / f"{name}.yml").write_text(manifest)


@pytest.fixture
def srcdir(tmp_path):
    board(tmp_path, "kit", "cores:\n  - m7\n  - name: m4\n    variants: [r2]\n")
    board(tmp_path, "plain", "variants: [wasm]\n")
    board(tmp_path, "rev", "revisions: [a]\ncores: [m7]\n")
    return tmp_path


@pytest.mark.parametrize(
    "target, revision, core, variant",
    [
        ("kit/m7", None, "m7", None),
        ("kit/m4/r2", None, "m4", "r2"),
        ("plain", None, None, None),
        ("plain/wasm", None, None, "wasm"),
        ("rev@a/m7", "a", "m7", None),
    ],
)
def test_parse(srcdir, target, revision, core, variant):
    spec = boards.parse_board(srcdir, target)
    assert (spec.revision, spec.core, spec.variant) == (revision, core, variant)


@pytest.mark.parametrize(
    "target, error",
    [
        ("kit", "requires a core"),
        ("kit/r2", "Unknown core"),
        ("kit/m7/m4", "Unknown variant"),
        ("kit/m7/r2", "Unknown variant 'r2' for core 'm7'"),
        ("kit/m7/r2/x", "expected BOARD"),
        ("kit//r2", "expected BOARD"),
        ("plain/wasm/x", "has no cores"),
        ("plain/m7", "Unknown variant"),
    ],
)
def test_parse_errors(srcdir, target, error):
    with pytest.raises(ValueError, match=error):
        boards.parse_board(srcdir, target)


def test_available(srcdir):
    assert boards.available_boards(srcdir) == [
        "kit/m7",
        "kit/m4",
        "kit/m4/r2",
        "plain",
        "plain/wasm",
        "rev@a/m7",
    ]


@pytest.mark.parametrize(
    "manifest, error",
    [
        ("cores: [m7]\nvariants: [r2]\n", "lists the variants of each core"),
        ("cores:\n  - name: m7\n    variants: [m7]\n", "both a core and one of its"),
        ("cores: [m7, m7]\n", "Duplicate cores"),
        ("cores:\n  - variants: [r2]\n", "must have a name"),
        ("cores:\n  - name: m7\n    soc: x\n", "a mapping with name and variants"),
    ],
)
def test_manifest_errors(tmp_path, manifest, error):
    board(tmp_path, "kit", manifest)
    with pytest.raises(ValueError, match=error):
        boards.parse_board(tmp_path, "kit/m7")
