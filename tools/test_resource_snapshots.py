#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Scott Anderson
# SPDX-License-Identifier: Apache-2.0

"""Regenerate or check the system resource snapshots the unit tests load.

The image tests draw with copies of each platform's system resource pack in
tests/fixtures/resources, with matching resource_ids.auto.h and
resource_version.auto.h overrides. This builds the packs from the current
resources and copies them over, or with --check, fails if the copies are out
of date.

Stored apps are compiled from code with the firmware toolchain, so --check
lets their bytes differ.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pbpack import ResourcePack

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIXTURES = os.path.join(ROOT, "tests", "fixtures", "resources")
OVERRIDES = os.path.join(ROOT, "tests", "overrides", "default", "resources")

# Test platform and the board whose resources it uses
PLATFORMS = {
    "emery": "obelix@pvt",
    "flint": "asterix",
    "gabbro": "getafix@dvt2",
}
PACK = "system_resources.pbpack"
IDS = "fw/resource/resource_ids.auto.h"
VERSION = "fw/resource/resource_version.auto.h"
IDS_RE = re.compile(r"RESOURCE_ID_(\w+) = (\d+),")
VERSION_RE = re.compile(r"\.crc = (\d+)")
UNCHECKED_RE = re.compile(r"^STORED_APP_")


def build(board, build_dir):
    subprocess.run(
        ["pbl", "configure", "-b", build_dir, "--board", board],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    subprocess.run(
        ["ninja", "-C", build_dir, PACK, IDS, VERSION],
        check=True,
        stdout=subprocess.DEVNULL,
    )


def snapshot_paths(platform):
    """Where the snapshot of each build output lives."""
    resource_dir = os.path.join(OVERRIDES, platform, "resource")
    return {
        PACK: os.path.join(FIXTURES, f"system_resources_{platform}.pbpack"),
        IDS: os.path.join(resource_dir, "resource_ids.auto.h"),
        VERSION: os.path.join(resource_dir, "resource_version.auto.h"),
    }


def read(path, mode="r"):
    with open(path, mode) as f:
        return f.read()


def load_pack(path):
    with open(path, "rb") as f:
        return ResourcePack.deserialize(f)


def contents(pack):
    return [pack.contents[e.content_index] for e in pack.table_entries]


def check(platform, build_dir):
    """Problems with the platform's snapshot, as text."""
    paths = snapshot_paths(platform)
    problems = []
    ids = read(os.path.join(build_dir, IDS))
    if read(paths[IDS]) != ids:
        problems.append(f"{os.path.relpath(paths[IDS], ROOT)} is out of date")
        return problems
    names = {int(n): name for name, n in IDS_RE.findall(ids)}

    committed = load_pack(paths[PACK])
    for i, (old, new) in enumerate(
        zip(contents(committed), contents(load_pack(os.path.join(build_dir, PACK)))),
        start=1,
    ):
        name = names.get(i, str(i))
        if old != new and not UNCHECKED_RE.match(name):
            problems.append(f"{name} differs in {os.path.relpath(paths[PACK], ROOT)}")

    # The version is the pack's CRC, so it has to match the committed pack
    crc = int(VERSION_RE.search(read(paths[VERSION])).group(1))
    if crc != committed.crc:
        problems.append(
            f"{os.path.relpath(paths[VERSION], ROOT)} doesn't match the committed pack"
        )
    return problems


def update(platform, build_dir):
    for output, path in snapshot_paths(platform).items():
        shutil.copyfile(os.path.join(build_dir, output), path)
    # Keep the version on one line, as the committed overrides have it
    path = snapshot_paths(platform)[VERSION]
    text = re.sub(
        r"\{\s*\.crc = (\d+),\s*\.timestamp = (\d+)\s*\}",
        r"{.crc = \1, .timestamp = \2}",
        read(path),
    )
    with open(path, "w") as f:
        f.write(text)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "--check", action="store_true", help="Fail if a snapshot is out of date"
    )
    args = p.parse_args()

    problems = []
    with tempfile.TemporaryDirectory() as tmp:
        for platform, board in PLATFORMS.items():
            build_dir = os.path.join(tmp, platform)
            build(board, build_dir)
            if args.check:
                problems += [f"{platform}: {p}" for p in check(platform, build_dir)]
            else:
                update(platform, build_dir)

    if problems:
        print("\n".join(problems), file=sys.stderr)
        print(
            "Run tools/test_resource_snapshots.py and update the reference images "
            "that change.",
            file=sys.stderr,
        )
        sys.exit(1)


if __name__ == "__main__":
    main()
