# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import contextlib
import json
import os
import time

from pbl import runners, samples
from pbl.command import PblCommand
from pbl.device import make_runner
from pbl.errors import CommandContextError

SAMPLES_DIR = "samples"
DEFAULT_BUILD_DIR = "build-samples"
DEFAULT_BOARD = "qemu_emery"
LOG_FILE = "samples.log"


class Samples(PblCommand):
    group = "build"

    def __init__(self):
        super().__init__(
            "samples",
            "Build the samples and check what they print",
            "Build each sample for a board, run it where the board runs on "
            "the host, such as an emulator, and check its console against "
            "the sample's sample.yml. Samples are built only for the other "
            "boards. With --integration, each sample is checked on the boards "
            "its sample.yml names for that.",
        )

    def do_add_parser(self, parser_adder):
        parser = self.add_subparser(parser_adder)
        parser.add_argument(
            "samples",
            nargs="*",
            metavar="SAMPLE",
            help="Sample, or directory of samples, e.g. samples/kernel "
            f"(default: all of {SAMPLES_DIR}/)",
        )
        parser.add_argument(
            "--board",
            metavar="BOARD",
            help=f"Board to build for (default: {DEFAULT_BOARD}); with "
            "--integration, only the samples that name it in their "
            "integration boards (default: each on all of its own)",
        )
        parser.add_argument(
            "-l", "--list", action="store_true", help="List the samples and exit"
        )
        parser.add_argument(
            "--list-boards",
            action="store_true",
            help="Print the boards the samples would be built for, as a JSON "
            "list, and exit",
        )
        parser.add_argument(
            "--integration",
            action="store_true",
            help="Check each sample on its integration boards, and only "
            "build those marked build_only",
        )
        parser.add_argument(
            "--build-only",
            action="store_true",
            help="Build the samples without running",
        )
        parser.add_argument(
            "-v",
            "--verbose",
            action="store_true",
            help="Show the console of the samples that pass too",
        )
        return parser

    def do_run(self, args, unknown):
        root = os.path.join(self.topdir, SAMPLES_DIR)
        try:
            found = samples.find(root, args.samples)
        except samples.SampleError as e:
            self.die(str(e))

        board = args.board
        if board is None and not args.integration:
            board = DEFAULT_BOARD
        try:
            pairs = samples.plan(found, board, args.integration, self._cores)
        except samples.SampleError as e:
            self.die(str(e))

        if args.list_boards:
            print(json.dumps(sorted({target for target, _ in pairs})))
            return 0

        if args.list:
            if args.integration:
                for target, sample in pairs:
                    print(f"{target}  {sample.name}")
                return 0
            width = max(len(s.name) for s in found)
            for sample in found:
                print(f"{sample.name:<{width}}  {sample.description}")
            return 0

        base = getattr(args, "build_dir", None) or os.path.join(
            self.topdir, DEFAULT_BUILD_DIR
        )
        failed = []
        counts = {}
        for target, sample in pairs:
            if target not in counts:
                counts[target] = {}
            name = sample.name if board else f"{target}: {sample.name}"
            reason = self._skip_reason(sample, target)
            if reason:
                outcome = "skipped"
            else:
                build_dir = os.path.join(
                    base,
                    target.replace("@", "_").replace("/", "_"),
                    *sample.name.split(os.sep),
                )
                start = time.monotonic()
                outcome = self._one(sample, target, build_dir, args)
                outcome_time = time.monotonic() - start
            counts[target][outcome] = counts[target].get(outcome, 0) + 1
            if outcome == "skipped":
                self.inf(f"SKIP  {name} ({reason})", color="yellow")
            elif outcome == "failed":
                failed.append(name)
                self.inf(f"FAIL  {name} ({outcome_time:.1f} s)", color="red")
            else:
                label = "PASS " if outcome == "passed" else "BUILT"
                self.inf(f"{label} {name} ({outcome_time:.1f} s)", color="green")

        if not pairs:
            self.inf(f"no sample to check on {board}", color="yellow")
        for target, outcomes in counts.items():
            summary = ", ".join(f"{n} {o}" for o, n in sorted(outcomes.items()))
            bad = "failed" in outcomes
            self.inf(f"{target}: {summary}", color="red" if bad else "green")
        if failed:
            self.inf(f"failed: {', '.join(failed)}", color="red")
        return 1 if failed else 0

    def _cores(self, name):
        from tools import boards

        return boards.load_cores(os.path.join(self.topdir, "boards", name), name)

    def _skip_reason(self, sample, board):
        if not sample.supports(board):
            return f"not for {board}"
        try:
            missing = sample.missing_roles(self.topdir, board)
        except samples.SampleError as e:
            self.die(str(e))
        if missing:
            return f"{board} has no {', '.join(missing)}"
        return None

    def _one(self, sample, board, build_dir, args):
        """Build and run one sample: "passed", "built" or "failed"."""
        from pbl.build import BuildDir

        build = BuildDir(build_dir, self.topdir)
        if self.dry_run:
            self.cmake_configure(
                build, board, app=sample.path, addons=sample.addons_for(board)
            )
            self.cmake_build(build)
            return "built"

        os.makedirs(build.path, exist_ok=True)
        log_path = build.join(LOG_FILE)
        with open(log_path, "w") as log:
            quiet = {"stdout": log, "stderr": log}
            rc = self.cmake_configure(
                build, board, app=sample.path, addons=sample.addons_for(board), **quiet
            )
            rc = rc or self.run_cmd(["cmake", "--build", str(build)], **quiet)
        if rc:
            self._show(log_path)
            return "failed"
        if args.build_only or (args.integration and sample.build_only):
            return "built"

        try:
            runner = make_runner(build, args, ("run",))
            with open(log_path, "a") as log:
                # Runners print the commands they run on the way.
                argv, env = self._quietly(log, runner.console_command)
        except (runners.UnsupportedOperation, CommandContextError):
            return "built"
        except runners.RunnerError as e:
            self.wrn(str(e))
            self._show(log_path)
            return "failed"

        result = samples.check(
            argv,
            sample.expect,
            sample.timeout,
            env=env,
            cwd=build.path,
            echo=print if args.verbose else None,
        )
        if result.passed:
            return "passed"

        if not args.verbose:
            print("\n".join(result.output), flush=True)
        if result.rebooted:
            why = "it booted again"
        elif result.returncode is not None:
            why = f"exited with status {result.returncode}"
        else:
            why = f"nothing in {sample.timeout:g} s"
        self.wrn(f"{sample.name}: no line matching {result.missing!r} ({why})")
        return "failed"

    @staticmethod
    def _quietly(log, fn):
        """Call ``fn`` with the output of the processes it starts, and its
        own prints, going to ``log``."""
        stdout = os.dup(1)
        try:
            with contextlib.redirect_stdout(log):
                os.dup2(log.fileno(), 1)
                return fn()
        finally:
            log.flush()
            os.dup2(stdout, 1)
            os.close(stdout)

    @staticmethod
    def _show(log_path):
        with open(log_path, errors="replace") as log:
            print("".join(log.readlines()[-60:]), end="", flush=True)
