# Copyright 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os

import yaml

NO_REVISION = "_"


class BoardSpec:
    def __init__(
        self,
        target,
        name,
        revision=None,
        core=None,
        variant=None,
        runners=None,
        qemu=None,
    ):
        self.target = target
        self.name = name
        self.revision = revision
        self.core = core
        self.variant = variant
        self.runners = runners or []
        self.qemu = qemu or {}

    @property
    def normalized(self):
        return self.target.replace("@", "_").replace("/", "_")


def _boards_dir(srcdir):
    return os.path.join(srcdir, "boards")


def _revision_manifest(board_dir, board):
    manifest = os.path.join(board_dir, f"{board}.yml")
    if os.path.exists(manifest):
        return manifest
    return None


def _load_manifest(board_dir, board):
    manifest = _revision_manifest(board_dir, board)
    if manifest is None:
        return None, {}

    with open(manifest) as f:
        data = yaml.safe_load(f)

    if not isinstance(data, dict):
        raise TypeError(f"Invalid revision manifest {manifest}: expected mapping")

    return manifest, data


def _validate_string_list(manifest, data, key):
    values = data.get(key, [])
    if not isinstance(values, list):
        raise TypeError(f"Invalid revision manifest {manifest}: expected {key} list")

    for value in values:
        if not isinstance(value, str) or not value:
            raise ValueError(
                f"Invalid revision manifest {manifest}: {key} must be non-empty strings"
            )

    if len(values) != len(set(values)):
        raise ValueError(f"Duplicate {key} found in {manifest}")

    return values


def load_revisions(board_dir, board):
    manifest, data = _load_manifest(board_dir, board)
    if manifest is None:
        return []

    revisions = data.get("revisions")
    if revisions is None:
        return []

    if not isinstance(revisions, list):
        raise TypeError(
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


def _load_cores(manifest, data):
    """The cores and their variants: each entry of ``cores`` is a name, or a mapping with its
    ``name`` and its ``variants``."""
    entries = data.get("cores", [])
    if not isinstance(entries, list):
        raise TypeError(f"Invalid revision manifest {manifest}: expected cores list")
    if entries and "variants" in data:
        raise ValueError(
            f"Invalid revision manifest {manifest}: "
            "a board with cores lists the variants of each core"
        )

    cores = {}
    for entry in entries:
        if isinstance(entry, str):
            entry = {"name": entry}
        if not isinstance(entry, dict) or set(entry) - {"name", "variants"}:
            raise ValueError(
                f"Invalid revision manifest {manifest}: "
                "a core is a name, or a mapping with name and variants"
            )
        name = entry.get("name")
        if not isinstance(name, str) or not name:
            raise ValueError(
                f"Invalid revision manifest {manifest}: cores must have a name"
            )
        if name in cores:
            raise ValueError(f"Duplicate cores found in {manifest}")
        variants = _validate_string_list(manifest, entry, "variants")
        if name in variants:
            raise ValueError(
                f"Invalid revision manifest {manifest}: "
                f"{name} both a core and one of its variants"
            )
        cores[name] = variants

    return cores


def load_variants(board_dir, board, core=None):
    """Other ways to build the board, or the given core of it, e.g. for another toolchain."""
    manifest, data = _load_manifest(board_dir, board)
    if manifest is None:
        return []

    if core is not None:
        return _load_cores(manifest, data).get(core, [])

    return _validate_string_list(manifest, data, "variants")


def load_cores(board_dir, board):
    """The cores of a multi-core board, each built for on its own as ``board/core``."""
    manifest, data = _load_manifest(board_dir, board)
    if manifest is None:
        return []

    return list(_load_cores(manifest, data))


def load_runners(board_dir, board):
    manifest, data = _load_manifest(board_dir, board)
    if manifest is None:
        return []

    return _validate_string_list(manifest, data, "runners")


def load_qemu(board_dir, board):
    """The board's emulator settings, e.g. the SDL decorations it ships."""
    manifest, data = _load_manifest(board_dir, board)
    if manifest is None:
        return {}

    qemu = data.get("qemu", {})
    if not isinstance(qemu, dict):
        raise TypeError(f"Invalid revision manifest {manifest}: expected qemu mapping")

    decorations = _validate_string_list(manifest, qemu, "decorations")
    return {"decorations": decorations}


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
        cores = load_cores(board_dir, board)
        targets = [f"{board}@{revision}" for revision in revisions] or [board]
        for target in targets:
            for core in cores or [None]:
                base = f"{target}/{core}" if core else target
                choices.append(base)
                choices.extend(
                    f"{base}/{variant}"
                    for variant in load_variants(board_dir, board, core)
                )

    return choices


def parse_board(srcdir, target):
    """The board of ``BOARD[@REVISION][/CORE][/VARIANT]``, where a board with cores takes one."""
    board, *qualifiers = target.split("/")
    if any(not qualifier for qualifier in qualifiers) or len(qualifiers) > 2:
        raise ValueError(
            f"Invalid board '{target}': expected BOARD[@REVISION][/CORE][/VARIANT]"
        )

    if "@" in board:
        board, revision = board.split("@", 1)
        if not board or not revision:
            raise ValueError(f"Invalid board '{target}': expected BOARD@REVISION")
    else:
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

    core = None
    cores = load_cores(board_dir, board)
    if cores:
        if not qualifiers:
            raise ValueError(
                f"Board '{board}' requires a core: "
                f"{', '.join(f'{board}/{c}' for c in cores)}"
            )
        core = qualifiers.pop(0)
        if core not in cores:
            raise ValueError(
                f"Unknown core '{core}' for board '{board}': {', '.join(cores)}"
            )
    elif len(qualifiers) > 1:
        raise ValueError(f"Board '{board}' has no cores: expected BOARD[/VARIANT]")

    variant = qualifiers[0] if qualifiers else None
    if variant is not None:
        variants = load_variants(board_dir, board, core)
        if variant not in variants:
            where = f"core '{core}' of board '{board}'" if core else f"board '{board}'"
            raise ValueError(
                f"Unknown variant '{variant}' for {where}: "
                f"{', '.join(variants) or 'none'}"
            )

    return BoardSpec(
        target,
        board,
        revision,
        core,
        variant,
        load_runners(board_dir, board),
        load_qemu(board_dir, board),
    )
