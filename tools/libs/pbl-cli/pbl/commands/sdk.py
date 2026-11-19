# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import filecmp
import json
import os
import shutil
import sys
import tarfile

from pbl.command import PblCommand

DEFAULT_BUILD_DIR = "build-sdk"
# The boards the SDK is packaged from, one for each platform.
BOARDS_FILE = os.path.join("sdk", "boards.yml")
SDK_DIR = "sdk-core"

# What the tool that installs the SDK reads beside the SDK itself.
TOP_LEVEL_FILES = ("package.json", "use_requirements.json", "requirements.txt")


class Sdk(PblCommand):
    group = "build"

    def __init__(self):
        super().__init__(
            "sdk",
            "Package the SDK of every platform",
            "Build the SDK of each platform from the board it is packaged "
            "from, add the frozen platforms, and assemble the sdk-core tree "
            "the pebble tool installs.",
        )

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        parser.add_argument(
            "version", metavar="VERSION", help="Version of the SDK, e.g. 4.44"
        )
        parser.add_argument(
            "--board",
            action="append",
            metavar="BOARD",
            help="Board to package a platform from; repeat for more "
            f"(default: the boards of {BOARDS_FILE})",
        )
        parser.add_argument(
            "--frozen",
            metavar="DIR",
            help="Directory with the libpebble.a and emulator of the frozen "
            "platforms, as <platform>/lib/libpebble.a and <platform>/qemu",
        )
        parser.add_argument(
            "--tarball",
            action="store_true",
            help=f"Also write {SDK_DIR}.tar.gz",
        )
        return parser

    def do_run(self, args, unknown):
        from pbl.build import BuildDir
        from tools.pebble_sdk_platform import pebble_platforms

        base = getattr(args, "build_dir", None) or os.path.join(
            self.topdir, DEFAULT_BUILD_DIR
        )
        base = os.path.abspath(base)
        names = args.board or self._boards()
        if args.frozen and not os.path.isdir(args.frozen):
            self.die(f"{args.frozen} is not a directory")

        out = os.path.join(base, SDK_DIR)
        pebble = os.path.join(out, "pebble")
        if not self.dry_run:
            shutil.rmtree(out, ignore_errors=True)

        # Each board in a build directory of its own.
        packaged = {}
        build = None
        for board in names:
            build = BuildDir(os.path.join(base, board.replace("@", "_")), self.topdir)
            # The firmware the SDK's emulator runs is the SDK shell.
            rc = self.cmake_configure(build, board, extra=["-DCONFIG_SHELL_SDK=y"])
            if rc:
                self.die(f"could not configure {board}", rc)
            self.cmake_build(build, "sdk")
            if self.dry_run:
                continue
            if build.config.CONFIG_QEMU:
                self.cmake_build(build, "sdk_emulator")

            platform = build.config.PLATFORM_NAME
            abi = build.config.CONFIG_PLATFORM_ABI
            if platform in packaged:
                self.die(f"{board} and {packaged[platform]} are both of {platform}")
            # The first build brings what the platforms share.
            if not packaged:
                shutil.copytree(build.join("sdk"), pebble, symlinks=True)
            else:
                shutil.copytree(
                    build.join("sdk", platform),
                    os.path.join(pebble, platform),
                    symlinks=True,
                )
                self._merge_abi(
                    build.join("sdk", abi), os.path.join(pebble, abi), board
                )
            packaged[platform] = board
            self.inf(f"Packaged {platform} from {board}")

        # The frozen platforms: headers and shims out of this checkout, at
        # the SDK revision each was frozen at.
        frozen = sorted(
            n for n, p in pebble_platforms.items() if "FROZEN_AT_REVISION" in p
        )
        frozen_dir = os.path.join(base, "frozen")
        if not self.dry_run:
            shutil.rmtree(frozen_dir, ignore_errors=True)
        self.check_cmd(
            [
                sys.executable,
                self.script("tools", "build_sdk.py"),
                *frozen,
                "--autoconf",
                build.join("autoconf.h"),
                "--output-dir",
                frozen_dir,
            ]
        )
        if self.dry_run:
            return 0
        for platform in frozen:
            shutil.copytree(
                os.path.join(frozen_dir, platform), os.path.join(pebble, platform)
            )

        # Their library and emulator are the ones they were frozen with.
        if args.frozen:
            for platform in frozen:
                self._add_frozen(args.frozen, pebble, platform)
        else:
            self.wrn(
                f"{', '.join(frozen)} have no libpebble.a and no emulator: "
                "give them with --frozen"
            )

        for name in TOP_LEVEL_FILES:
            shutil.move(os.path.join(pebble, name), os.path.join(out, name))
        with open(os.path.join(out, "use_requirements.json")) as f:
            requirements = json.load(f)
        with open(os.path.join(out, "manifest.json"), "w") as f:
            json.dump(
                {
                    "requirements": requirements,
                    "version": args.version,
                    "type": "sdk-core",
                    "channel": "",
                },
                f,
                indent=4,
            )
            f.write("\n")
        self.inf(
            f"SDK {args.version} for {', '.join(sorted([*packaged, *frozen]))} in {out}"
        )

        if args.tarball:
            tarball = out + ".tar.gz"
            with tarfile.open(tarball, "w:gz") as tar:
                tar.add(out, arcname=SDK_DIR)
            self.inf(f"Wrote {tarball}")
        return 0

    def _boards(self):
        import yaml

        path = os.path.join(self.topdir, BOARDS_FILE)
        with open(path) as f:
            names = (yaml.safe_load(f) or {}).get("boards")
        if not isinstance(names, list) or not all(isinstance(n, str) for n in names):
            self.die(f"{path} does not list the boards to package the SDK from")
        if not names:
            self.die(f"{path} lists no board")
        return names

    def _merge_abi(self, source, dest, board):
        """The headers and library of an ABI are the same from whichever
        board they are built: a build only adds what the others lack, such
        as the headers of a feature its platform alone has."""
        if not os.path.isdir(dest):
            shutil.copytree(source, dest)
            return
        for root, _, files in os.walk(source):
            rel = os.path.relpath(root, source)
            os.makedirs(os.path.join(dest, rel), exist_ok=True)
            for name in files:
                src = os.path.join(root, name)
                dst = os.path.join(dest, rel, name)
                if not os.path.exists(dst):
                    shutil.copy2(src, dst)
                # The Doxyfile names the build directory it was written in.
                elif not name.startswith("Doxyfile") and not filecmp.cmp(
                    src, dst, shallow=False
                ):
                    self.die(
                        f"{os.path.normpath(os.path.join(rel, name))} of {board} "
                        "differs from that of the boards before it"
                    )

    def _add_frozen(self, source, pebble, platform):
        lib = os.path.join(source, platform, "lib", "libpebble.a")
        qemu = os.path.join(source, platform, "qemu")
        if not os.path.isfile(lib) or not os.path.isdir(qemu):
            self.die(f"{source} has no lib/libpebble.a and qemu for {platform}")
        shutil.copy2(lib, os.path.join(pebble, platform, "lib", "libpebble.a"))
        shutil.copytree(qemu, os.path.join(pebble, platform, "qemu"))
