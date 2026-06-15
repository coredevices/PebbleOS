# Copyright 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os

import yaml


NO_REVISION = "_"


class BoardSpec:
    def __init__(self, target, name, revision=None):
        self.target = target
        self.name = name
        self.revision = revision


def _boards_dir(srcdir):
    return os.path.join(srcdir, "boards")


def _revision_manifest(board_dir, board):
    manifest = os.path.join(board_dir, f"{board}.yml")
    if os.path.exists(manifest):
        return manifest
    return None


def load_revisions(board_dir, board):
    manifest = _revision_manifest(board_dir, board)
    if manifest is None:
        return []

    with open(manifest) as f:
        data = yaml.safe_load(f)

    if not isinstance(data, dict):
        raise ValueError(f"Invalid revision manifest {manifest}: expected mapping")

    revisions = data.get("revisions")
    if not isinstance(revisions, list):
        raise ValueError(
            f"Invalid revision manifest {manifest}: expected revisions list"
        )

    if not revisions:
        raise ValueError(f"No revisions found in {manifest}")

    for revision in revisions:
        if not isinstance(revision, str) or not revision:
            raise ValueError(
                f"Invalid revision manifest {manifest}: "
                "revisions must be non-empty strings"
            )

    if len(revisions) != len(set(revisions)):
        raise ValueError(f"Duplicate revisions found in {manifest}")

    return revisions


def available_boards(srcdir):
    choices = []
    for board in sorted(os.listdir(_boards_dir(srcdir))):
        board_dir = os.path.join(_boards_dir(srcdir), board)
        if not os.path.isdir(board_dir) or board.startswith("."):
            continue
        if not _revision_manifest(board_dir, board) and not os.path.exists(
            os.path.join(board_dir, "defconfig")
        ):
            continue

        revisions = load_revisions(board_dir, board)
        if revisions:
            choices.extend(f"{board}@{revision}" for revision in revisions)
        else:
            choices.append(board)

    return choices


def parse_board(srcdir, target):
    if "@" in target:
        board, revision = target.split("@", 1)
        if not board or not revision:
            raise ValueError(f"Invalid board '{target}': expected BOARD@REVISION")
    else:
        board = target
        revision = None

    board_dir = os.path.join(_boards_dir(srcdir), board)
    if not os.path.isdir(board_dir):
        raise ValueError(f"Unknown board '{target}'")
    if not _revision_manifest(board_dir, board) and not os.path.exists(
        os.path.join(board_dir, "defconfig")
    ):
        raise ValueError(f"Unknown board '{target}'")

    revisions = load_revisions(board_dir, board)
    if revisions:
        if revision is None:
            raise ValueError(
                f"Board '{board}' requires a revision: "
                f"{', '.join(f'{board}@{r}' for r in revisions)}"
            )
        if revision not in revisions:
            raise ValueError(
                f"Unknown revision '{revision}' for board '{board}': "
                f"{', '.join(revisions)}"
            )
    elif revision is not None:
        raise ValueError(f"Board '{board}' does not define revisions")

    return BoardSpec(target, board, revision)
