# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The samples: finding them, and checking what one prints when it runs.

A sample is a directory with a ``sample.yml`` next to its ``CMakeLists.txt``:

.. code-block:: yaml

    description: One line on what the sample shows
    expect:
      - "regular expression a console line has to match"
      - "then this one, on a later line"
    timeout: 10
    boards: [qemu_emery]
    skip: [native_emery]
    roles: [accel]
    addons: [sifli_lcm_1p85]
    integration: [qemu_emery, obelix@pvt]
    build_only: true

``expect`` is what makes the sample a test: the lines its console has to
print, in that order, with any others in between, and before it boots a
second time: a sample that panics and starts over fails. ``timeout`` is how many
seconds it gets (default 10). ``boards`` limits it to the boards that have
what it needs (default: all of them), and ``skip`` names those that lack it.
``roles`` names the roles it uses: boards without them, after the board's and
the sample's roles files, are skipped too. ``addons`` are plugged into the
board, from ``boards/addons``, and give it roles too: a list for every board,
or a mapping of board names to lists, as ``{sf32lb57_dpi_hdk: [sifli_lcm_1p85]}``,
for boards that take different ones. ``integration`` names
the boards it covers enough to check on, with their revision for boards that
have revisions: with ``--integration`` it is built and run on each of those,
and a sample naming none is an error. With ``build_only`` it is only built.
``boards``, ``skip`` and ``integration`` name a board, its core or variant as
``board/core`` or ``board/variant``, or both as ``board/core/variant``.
"""

import os
import queue
import re
import subprocess
import threading
import time
from dataclasses import dataclass

import yaml

SAMPLE_FILE = "sample.yml"
DEFAULT_TIMEOUT = 10

_ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
_BOOT = re.compile(r"\*\*\* Booting PebbleOS ")
_KEYS = {
    "description",
    "expect",
    "timeout",
    "boards",
    "skip",
    "roles",
    "addons",
    "integration",
    "build_only",
}


class SampleError(Exception):
    pass


@dataclass(frozen=True)
class Sample:
    #: Path relative to the directory it was found from, e.g. ``kernel/work``.
    name: str
    path: str
    description: str
    expect: tuple
    timeout: float = DEFAULT_TIMEOUT
    boards: tuple = ()
    skip: tuple = ()
    roles: tuple = ()
    addons: tuple | dict = ()
    integration: tuple = ()
    build_only: bool = False

    @staticmethod
    def _names(board):
        """The names a sample lists ``board``, given as ``name[@revision][/core][/variant]``, by:
        the name, then with its core or variant, then with both."""
        target, *qualifiers = board.split("/")
        name = target.partition("@")[0]
        return tuple(
            "/".join([name, *qualifiers[:n]]) for n in range(len(qualifiers) + 1)
        )

    def supports(self, board):
        """Whether the sample is for ``board``, given as ``name[@revision][/core][/variant]``."""
        names = self._names(board)
        if any(name in self.skip for name in names):
            return False
        return not self.boards or any(name in self.boards for name in names)

    def integrates(self, board):
        """Whether ``board`` is one to check the sample on in integration mode."""
        return any(_names_board(entry, board) for entry in self.integration)

    def addons_for(self, board):
        """The add-ons plugged into ``board``, given as ``name[@revision][/core]``."""
        if isinstance(self.addons, dict):
            name = board.partition("/")[0].partition("@")[0]
            return tuple(self.addons.get(name, ()))
        return self.addons

    def missing_roles(self, srcdir, board):
        """The roles the sample uses that ``board`` gives no device."""
        if not self.roles:
            return []
        from tools import boards
        from tools.devicetree import dtmap

        try:
            spec = boards.parse_board(srcdir, board)
            files = dtmap.role_files(
                srcdir,
                spec.name,
                spec.revision,
                self.path,
                self.addons_for(board),
                spec.core,
                spec.variant,
            )
        except (ValueError, dtmap.DtError) as e:
            raise SampleError(str(e)) from e
        try:
            have = dtmap.merge_roles(files)
        except dtmap.DtError as e:
            raise SampleError(str(e)) from e
        return [role for role in self.roles if role not in have]


def _names_board(entry, board):
    """Whether an entry of a board list, ``name[@revision][/core][/variant]``, names ``board``:
    an entry naming a board alone, or without its revision, core or variant, names them all."""
    target, *qualifiers = entry.split("/")
    name, _, revision = target.partition("@")
    board_target, *board_qualifiers = board.split("/")
    board_name, _, board_revision = board_target.partition("@")
    return (
        name == board_name
        and (not revision or revision == board_revision)
        and board_qualifiers[: len(qualifiers)] == qualifiers
    )


def _targets(entry, cores):
    """The boards an integration entry stands for: each core of a board with cores named
    without one."""
    target, *qualifiers = entry.split("/")
    names = cores(target.partition("@")[0]) if not qualifiers else []
    return [f"{entry}/{core}" for core in names] or [entry]


def plan(found, board=None, integration=False, cores=lambda name: []):
    """The (board, sample) pairs to build, by board, in sample order within a board.

    Without ``integration`` every sample is for ``board``. With it, each sample is for each of
    its integration boards, an entry naming a board with cores, ``cores(name)``, standing for
    each of them, and ``board``, if given, keeps only those naming it.
    """
    if not integration:
        return [(board, sample) for sample in found]
    missing = [sample.name for sample in found if not sample.integration]
    if missing:
        raise SampleError(f"samples naming no integration board: {', '.join(missing)}")
    pairs = []
    for sample in found:
        if board is not None:
            if sample.integrates(board):
                pairs.append((board, sample))
        else:
            for entry in sample.integration:
                pairs += [(target, sample) for target in _targets(entry, cores)]
    return sorted(pairs, key=lambda pair: pair[0])


def load(path, name=None):
    """Read the sample in directory ``path``."""
    manifest = os.path.join(path, SAMPLE_FILE)
    try:
        with open(manifest) as f:
            data = yaml.safe_load(f)
    except (OSError, yaml.YAMLError) as e:
        raise SampleError(f"cannot read {manifest}: {e}") from e
    if not isinstance(data, dict):
        raise SampleError(f"invalid {manifest}: expected a mapping")

    unknown = set(data) - _KEYS
    if unknown:
        raise SampleError(
            f"invalid {manifest}: unknown keys {', '.join(sorted(unknown))}"
        )

    description = data.get("description")
    if not isinstance(description, str) or not description.strip():
        raise SampleError(f"invalid {manifest}: description must be a string")

    expect = data.get("expect")
    if (
        not isinstance(expect, list)
        or not expect
        or not all(isinstance(e, str) for e in expect)
    ):
        raise SampleError(
            f"invalid {manifest}: expect must be a list of regular expressions"
        )
    for pattern in expect:
        try:
            re.compile(pattern)
        except re.error as e:
            raise SampleError(
                f"invalid {manifest}: bad expression {pattern!r}: {e}"
            ) from e

    timeout = data.get("timeout", DEFAULT_TIMEOUT)
    if (
        isinstance(timeout, bool)
        or not isinstance(timeout, (int, float))
        or timeout <= 0
    ):
        raise SampleError(f"invalid {manifest}: timeout must be a number of seconds")

    lists = {}
    for key, what in (
        ("boards", "board"),
        ("skip", "board"),
        ("roles", "role"),
        ("integration", "board"),
    ):
        lists[key] = data.get(key, [])
        if not isinstance(lists[key], list) or not all(
            isinstance(b, str) for b in lists[key]
        ):
            raise SampleError(
                f"invalid {manifest}: {key} must be a list of {what} names"
            )

    addons = data.get("addons", [])
    per_board = isinstance(addons, dict)
    if not all(
        isinstance(k, str)
        and isinstance(v, list)
        and all(isinstance(a, str) for a in v)
        for k, v in (addons.items() if per_board else [("", addons)])
    ):
        raise SampleError(
            f"invalid {manifest}: addons must be a list of add-on names, or a mapping "
            "of board names to such lists"
        )

    build_only = data.get("build_only", False)
    if not isinstance(build_only, bool):
        raise SampleError(f"invalid {manifest}: build_only must be true or false")

    sample = Sample(
        name=name or os.path.basename(os.path.abspath(path)),
        path=os.path.abspath(path),
        description=description.strip(),
        expect=tuple(expect),
        timeout=timeout,
        boards=tuple(lists["boards"]),
        skip=tuple(lists["skip"]),
        roles=tuple(lists["roles"]),
        addons={k: tuple(v) for k, v in addons.items()} if per_board else tuple(addons),
        integration=tuple(lists["integration"]),
        build_only=build_only,
    )
    unsupported = [b for b in sample.integration if not sample.supports(b)]
    if unsupported:
        raise SampleError(
            f"invalid {manifest}: integration boards it is not for: "
            f"{', '.join(unsupported)}"
        )
    return sample


def find(root, paths=()):
    """The samples under ``root``, or only those in or under ``paths``,
    sorted by name."""
    root = os.path.abspath(root)
    found = {}
    for start in [os.path.abspath(p) for p in paths] or [root]:
        if not os.path.isdir(start):
            raise SampleError(f"no such directory: {start}")
        before = len(found)
        for dirpath, dirnames, filenames in os.walk(start):
            if SAMPLE_FILE in filenames:
                # Samples do not nest.
                dirnames.clear()
                found[dirpath] = load(dirpath, os.path.relpath(dirpath, root))
            dirnames.sort()
        if len(found) == before and start not in found:
            raise SampleError(f"no sample in {start}: no {SAMPLE_FILE} found")
    return sorted(found.values(), key=lambda s: s.name)


@dataclass
class Result:
    passed: bool
    #: The console, one entry per line.
    output: list
    #: The expression that no line matched in time, None when all did.
    missing: str | None = None
    #: Exit status when the program ended on its own before that.
    returncode: int | None = None
    #: Whether it booted again before that.
    rebooted: bool = False


def check(argv, expect, timeout, env=None, cwd=None, echo=None):
    """Run ``argv`` until its output has matched every expression in
    ``expect``, in order, or ``timeout`` seconds have passed. ``echo`` is
    called with each line as it arrives."""
    patterns = [re.compile(p) for p in expect]
    lines = queue.Queue()

    proc = subprocess.Popen(
        [str(a) for a in argv],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env={**os.environ, **(env or {})},
        cwd=cwd,
    )

    def read():
        for raw in proc.stdout:
            lines.put(raw)
        lines.put(None)

    reader = threading.Thread(target=read, daemon=True)
    reader.start()

    output = []
    matched = 0
    boots = 0
    returncode = None
    deadline = time.monotonic() + timeout
    try:
        while matched < len(patterns):
            try:
                raw = lines.get(timeout=max(deadline - time.monotonic(), 0))
            except queue.Empty:
                break
            if raw is None:
                returncode = proc.wait()
                break
            line = _ANSI.sub("", raw.decode(errors="replace")).replace("\0", "").strip()
            if not line:
                continue
            output.append(line)
            if echo:
                echo(line)
            if _BOOT.search(line):
                boots += 1
                if boots > 1:
                    break
            if patterns[matched].search(line):
                matched += 1
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        reader.join(timeout=5)

    passed = matched == len(patterns)
    return Result(
        passed=passed,
        output=output,
        missing=None if passed else expect[matched],
        returncode=returncode,
        rebooted=boots > 1,
    )
