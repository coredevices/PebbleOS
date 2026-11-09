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

``expect`` is what makes the sample a test: the lines its console has to
print, in that order, with any others in between, and before it boots a
second time: a sample that panics and starts over fails. ``timeout`` is how many
seconds it gets (default 10). ``boards`` limits it to the boards that have
what it needs (default: all of them), and ``skip`` names those that lack it.
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
_KEYS = {"description", "expect", "timeout", "boards", "skip"}


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

    def supports(self, board):
        """Whether the sample is for ``board``, given as ``name[@revision]``."""
        name = board.partition("@")[0]
        return name not in self.skip and (not self.boards or name in self.boards)


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
    for key in ("boards", "skip"):
        lists[key] = data.get(key, [])
        if not isinstance(lists[key], list) or not all(
            isinstance(b, str) for b in lists[key]
        ):
            raise SampleError(
                f"invalid {manifest}: {key} must be a list of board names"
            )

    return Sample(
        name=name or os.path.basename(os.path.abspath(path)),
        path=os.path.abspath(path),
        description=description.strip(),
        expect=tuple(expect),
        timeout=timeout,
        boards=tuple(lists["boards"]),
        skip=tuple(lists["skip"]),
    )


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
