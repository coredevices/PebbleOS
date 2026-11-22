# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "libs" / "pbl-cli"))

from pbl import samples


def sample(name, integration=()):
    return samples.Sample(name, name, name, ("x",), integration=tuple(integration))


@pytest.fixture
def found():
    return [
        sample("a", ["qemu_emery", "obelix@pvt"]),
        sample("b", ["qemu_emery"]),
    ]


def test_integration_runs_each_sample_on_its_boards(found):
    pairs = samples.plan(found, integration=True)
    assert [(board, s.name) for board, s in pairs] == [
        ("obelix@pvt", "a"),
        ("qemu_emery", "a"),
        ("qemu_emery", "b"),
    ]


@pytest.mark.parametrize(
    "board, names",
    [
        ("qemu_emery", ["a", "b"]),
        ("obelix@pvt", ["a"]),
        ("obelix@dvt", []),
        ("asterix", []),
    ],
)
def test_board_filters_integration(found, board, names):
    pairs = samples.plan(found, board, integration=True)
    assert all(target == board for target, _ in pairs)
    assert [s.name for _, s in pairs] == names


def test_entry_without_revision_names_every_revision():
    assert sample("a", ["obelix"]).integrates("obelix@dvt")
    assert not sample("a", ["obelix@pvt"]).integrates("obelix@dvt")


def test_integration_entry_without_core_stands_for_each_core():
    found = [sample("a", ["disco", "qemu_emery"]), sample("b", ["disco/m7"])]
    pairs = samples.plan(
        found, integration=True, cores=lambda n: ["m4", "m7"] if n == "disco" else []
    )
    assert [(board, s.name) for board, s in pairs] == [
        ("disco/m4", "a"),
        ("disco/m7", "a"),
        ("disco/m7", "b"),
        ("qemu_emery", "a"),
    ]


def test_entry_without_core_names_every_core():
    assert sample("a", ["disco"]).integrates("disco/m4")
    assert sample("a", ["disco/m7"]).integrates("disco/m7")
    assert not sample("a", ["disco/m7"]).integrates("disco/m4")
    assert not sample("a", ["disco/m7"]).integrates("disco")


def test_without_integration_every_sample_is_for_the_board(found):
    pairs = samples.plan(found, "asterix")
    assert [(board, s.name) for board, s in pairs] == [
        ("asterix", "a"),
        ("asterix", "b"),
    ]


def test_integration_rejects_sample_without_boards(found):
    with pytest.raises(samples.SampleError, match="board: c$"):
        samples.plan(found + [sample("c")], integration=True)
    assert samples.plan(found + [sample("c")], "asterix")[-1][1].name == "c"
