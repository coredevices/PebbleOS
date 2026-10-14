# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import re
import subprocess
import sys

from pbl.command import CommandContextError, PblCommand

ENTRY_RE = re.compile(r"^(?:(CONFIG_\w+)=.*|# (CONFIG_\w+) is not set)$")


def read_entries(path):
    """Symbol assignments of a .config file, by name."""
    entries = {}
    if os.path.isfile(path):
        with open(path) as f:
            for line in f:
                line = line.rstrip("\n")
                m = ENTRY_RE.match(line)
                if m:
                    entries[m.group(1) or m.group(2)] = line
    return entries


def save_changes(before, after, fragment):
    """Add the assignments that differ between two .config contents to the
    fragment, keeping the ones of earlier sessions. Returns whether the
    fragment changed."""
    changes = {name: line for name, line in after.items() if before.get(name) != line}
    if not changes:
        return False
    entries = read_entries(fragment)
    entries.update(changes)
    with open(fragment, "w") as f:
        f.writelines(line + "\n" for line in entries.values())
    return True


class Menuconfig(PblCommand):
    group = "build"

    def __init__(self):
        super().__init__(
            "menuconfig",
            "Edit the configuration interactively",
            "Edit the build's Kconfig configuration, then reconfigure with it. "
            "The changes are kept across reconfigurations in the build's "
            "menuconfig.conf; delete it to go back to the board defaults.",
        )

    def do_add_parser(self, parser_adder):
        return self.add_subparser(parser_adder)

    def do_run(self, args, unknown):
        from tools import boards

        build = self.build_dir()
        dotconfig = build.join(".config")
        if not os.path.isfile(dotconfig):
            raise CommandContextError(f"no .config in {build}")

        spec = build.board_spec
        env = dict(
            os.environ,
            srctree=self.topdir,
            BOARD=spec.name,
            BOARD_REVISION=spec.revision or boards.NO_REVISION,
            KCONFIG_CONFIG=dotconfig,
        )
        if self.dry_run:
            self.inf("[dry-run] menuconfig", dotconfig, color="yellow")
            return 0

        before = read_entries(dotconfig)
        subprocess.run(
            [sys.executable, "-m", "menuconfig", os.path.join(self.topdir, "Kconfig")],
            env=env,
            check=False,
        )

        if not save_changes(
            before, read_entries(dotconfig), build.join("menuconfig.conf")
        ):
            self.inf("no Kconfig changes")
            return 0
        return self.run_cmd(["cmake", str(build)])
